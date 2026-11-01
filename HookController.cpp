#include "HookController.h"
#include <cstdio>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

static void LogDebug(const wchar_t* msg) {
    FILE* f = _wfopen(L"C:\\Users\\Public\\sonicboost.log", L"a, ccs=UTF-8");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fwprintf(f, L"[%02d:%02d:%02d.%03d] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg);
        fflush(f);
        fclose(f);
    }
}

HookController::HookController()
    : m_hModule(NULL),
      m_pManager(nullptr),
      m_hWndMaster(NULL),
      m_pfnInitWindow(nullptr),
      m_pfnDestroyWindow(nullptr),
      m_pfnHookInstall(nullptr),
      m_pfnHookUninstall(nullptr),
      m_pfnIsHookInstalled(nullptr),
      m_pfnIsEnabled(nullptr),
      m_pfnSetEnabled(nullptr),
      m_pfnGetVolume(nullptr),
      m_pfnSetVolume(nullptr),
      m_pfnGetSuperBoost(nullptr),
      m_pfnSetSuperBoost(nullptr),
      m_pfnGetBoostMethod(nullptr),
      m_pfnSetBoostMethod(nullptr),
      m_volumePercent(100),
      m_enabled(true),
      m_superBoost(true),
      m_boostMethod(2)
{
    InitializeCriticalSection(&m_cs);
}

HookController::~HookController() {
    Shutdown();
    DeleteCriticalSection(&m_cs);
}

void HookController::EnsureSharedMemory(int volumePercent, bool enabled, bool superBoost, int boostMethod) {
    HANDLE hMap = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"CKLHookShared");
    if (!hMap) {
        hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0x1000, L"CKLHookShared");
    }
    if (hMap) {
        void* pBuf = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0x200);
        if (pBuf) {
            DWORD* pDwords = (DWORD*)pBuf;
            if (!enabled && volumePercent == 100 && m_pManager == nullptr) {
                // Application is cleanly shutting down: clear master window handle, dead hooks, and PID
                *((HWND*)pBuf) = NULL;
                pDwords[0x10 / 4] = 0; // Clear HHOOK
                pDwords[0x14 / 4] = 0;
                pDwords[0x18 / 4] = 0; // Disabled
                pDwords[0xA0 / 4] = 100; // Reset volume to 100%
                pDwords[0xA4 / 4] = 0;   // Reset PID
                pDwords[0xA8 / 4] = 0;   // SuperBoost off
                pDwords[0xB0 / 4] = 0;   // BoostMethod = 0
            } else {
                if (m_hWndMaster && IsWindow(m_hWndMaster)) {
                    *((HWND*)pBuf) = m_hWndMaster;
                }
                pDwords[0x18 / 4] = enabled ? 1 : 0;
                pDwords[0xA0 / 4] = (DWORD)volumePercent;
                pDwords[0xA4 / 4] = GetCurrentProcessId();
                pDwords[0xA8 / 4] = superBoost ? 1 : 0;
                pDwords[0xB0 / 4] = (DWORD)boostMethod;
            }
            UnmapViewOfFile(pBuf);
        }
        CloseHandle(hMap);
    }
}

bool HookController::Initialize(HWND hWndMaster) {
    EnterCriticalSection(&m_cs);

    if (m_pManager) {
        LeaveCriticalSection(&m_cs);
        return true;
    }

    m_hWndMaster = hWndMaster;

    // CRITICAL: Pre-initialize and sanitize CKLHookShared BEFORE loading SBH64.dll or calling KLHGetManager!
    // 1. Any HHOOK at offset 0x10 from a prior terminated process is dead in Windows OS.
    //    If left non-zero, HookInstall thinks the hook is already installed and SKIPS SetWindowsHookExW!
    // 2. Setting BoostMethod to 0 ensures CVSTManager (local limit.dll) is used, avoiding reliance on external service pipes.
    HANDLE hMap = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"CKLHookShared");
    if (!hMap) {
        hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0x1000, L"CKLHookShared");
    }
    if (hMap) {
        void* pBuf = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0x200);
        if (pBuf) {
            *((HWND*)pBuf) = m_hWndMaster;
            DWORD* pDwords = (DWORD*)pBuf;
            pDwords[0x10 / 4] = 0; // Clear stale HHOOK (lower 32 bits)
            pDwords[0x14 / 4] = 0; // Clear stale HHOOK (upper 32 bits)
            pDwords[0x18 / 4] = m_enabled.load() ? 1 : 0;
            pDwords[0xA0 / 4] = (DWORD)m_volumePercent.load();
            pDwords[0xA4 / 4] = GetCurrentProcessId();
            pDwords[0xA8 / 4] = m_superBoost.load() ? 1 : 0;
            pDwords[0xB0 / 4] = (DWORD)m_boostMethod.load();
            UnmapViewOfFile(pBuf);
        }
        CloseHandle(hMap);
    }

    WCHAR szModule[MAX_PATH];
    GetModuleFileNameW(NULL, szModule, MAX_PATH);
    WCHAR* pSlash = wcsrchr(szModule, L'\\');
    if (pSlash) *pSlash = L'\0';

    std::wstring localDll = std::wstring(szModule) + L"\\SBH64.dll";
    m_hModule = LoadLibraryW(localDll.c_str());

    if (!m_hModule) {
        m_hModule = LoadLibraryW(L"SBH64.dll");
    }

    if (!m_hModule) {
        LogDebug(L"[Hook] Failed to load SBH64.dll");
        LeaveCriticalSection(&m_cs);
        return false;
    }

    PFN_KLHGetManager pfnGetMgr = (PFN_KLHGetManager)GetProcAddress(m_hModule, "KLHGetManager");
    if (!pfnGetMgr) {
        LogDebug(L"[Hook] GetProcAddress KLHGetManager failed");
        FreeLibrary(m_hModule);
        m_hModule = NULL;
        LeaveCriticalSection(&m_cs);
        return false;
    }

    m_pManager = pfnGetMgr();
    if (!m_pManager) {
        LogDebug(L"[Hook] KLHGetManager returned NULL");
        FreeLibrary(m_hModule);
        m_hModule = NULL;
        LeaveCriticalSection(&m_cs);
        return false;
    }

    wchar_t szBuf[128];
    swprintf_s(szBuf, L"[Hook] SBH64.dll loaded (0x%p), Manager at 0x%p", (void*)m_hModule, m_pManager);
    LogDebug(szBuf);

    // Resolve vtable function pointers directly with true method offsets
    void** vtbl = *(void***)m_pManager;
    m_pfnInitWindow      = (PFN_InitWindow)vtbl[1];
    m_pfnDestroyWindow   = (PFN_DestroyWindow)vtbl[2];
    m_pfnHookInstall     = (PFN_HookInstall)vtbl[3];
    m_pfnHookUninstall   = (PFN_HookUninstall)vtbl[4];
    m_pfnIsHookInstalled = (PFN_IsHookInstalled)vtbl[5];
    m_pfnIsEnabled       = (PFN_IsEnabled)vtbl[6];
    m_pfnSetEnabled      = (PFN_SetEnabled)vtbl[7];
    m_pfnGetVolume       = (PFN_GetVolume)vtbl[8];
    m_pfnSetVolume       = (PFN_SetVolume)vtbl[9];
    m_pfnGetSuperBoost   = (PFN_GetSuperBoost)vtbl[10];
    m_pfnSetSuperBoost   = (PFN_SetSuperBoost)vtbl[11];
    m_pfnGetBoostMethod  = (PFN_GetBoostMethod)vtbl[12];
    m_pfnSetBoostMethod  = (PFN_SetBoostMethod)vtbl[13];

    // Initialize controller window and IPC with real top-level controller window
    if (m_pfnInitWindow) {
        m_pfnInitWindow(m_pManager, hWndMaster);
    }

    // Install the global hook across all processes
    bool bInstalled = false;
    if (m_pfnHookInstall) {
        bInstalled = m_pfnHookInstall(m_pManager);
    }
    bool bIsInstalled = m_pfnIsHookInstalled ? m_pfnIsHookInstalled(m_pManager) : false;

    swprintf_s(szBuf, L"[Hook] HookInstall returned: %s, IsHookInstalled: %s",
               bInstalled ? L"TRUE" : L"FALSE", bIsInstalled ? L"TRUE" : L"FALSE");
    LogDebug(szBuf);

    // Apply initial settings
    if (m_pfnSetEnabled) {
        m_pfnSetEnabled(m_pManager, m_enabled.load());
    }
    if (m_pfnSetVolume) {
        m_pfnSetVolume(m_pManager, m_volumePercent.load());
    }
    if (m_pfnSetSuperBoost) {
        m_pfnSetSuperBoost(m_pManager, (void*)(uintptr_t)(m_superBoost.load() ? 1 : 0));
    }
    if (m_pfnSetBoostMethod) {
        m_pfnSetBoostMethod(m_pManager, m_boostMethod.load());
    }

    // Synchronize shared memory bridge
    EnsureSharedMemory(m_volumePercent.load(), m_enabled.load(), m_superBoost.load(), m_boostMethod.load());

    LeaveCriticalSection(&m_cs);
    return true;
}

bool HookController::IsHookInstalled() const {
    if (m_pManager && m_pfnIsHookInstalled) {
        return m_pfnIsHookInstalled(m_pManager);
    }
    return false;
}

void HookController::Shutdown() {
    EnterCriticalSection(&m_cs);

    if (m_pManager) {
        if (m_pfnSetVolume) {
            m_pfnSetVolume(m_pManager, 100);
        }
        if (m_pfnHookUninstall) {
            m_pfnHookUninstall(m_pManager);
        }
        if (m_pfnDestroyWindow) {
            m_pfnDestroyWindow(m_pManager);
        }
        m_pManager = nullptr;
    }

    // Reset shared memory state cleanly
    EnsureSharedMemory(100, false, false, 2);

    if (m_hModule) {
        FreeLibrary(m_hModule);
        m_hModule = NULL;
    }

    m_hWndMaster = NULL;

    LogDebug(L"[Hook] HookController shutdown cleanly.");

    LeaveCriticalSection(&m_cs);
}

void HookController::SetVolume(int volumePercent) {
    if (volumePercent < 100) volumePercent = 100;
    if (volumePercent > 500) volumePercent = 500;
    m_volumePercent = volumePercent;

    EnterCriticalSection(&m_cs);
    if (m_pManager && m_pfnSetVolume) {
        m_pfnSetVolume(m_pManager, volumePercent);
    }
    EnsureSharedMemory(volumePercent, m_enabled.load(), m_superBoost.load(), m_boostMethod.load());
    LeaveCriticalSection(&m_cs);
}

void HookController::SetEnabled(bool enabled) {
    m_enabled = enabled;

    EnterCriticalSection(&m_cs);
    if (m_pManager && m_pfnSetEnabled) {
        m_pfnSetEnabled(m_pManager, enabled);
        if (!enabled && m_pfnSetVolume) {
            m_pfnSetVolume(m_pManager, 100);
        } else if (m_pfnSetVolume) {
            m_pfnSetVolume(m_pManager, m_volumePercent.load());
        }
    }
    EnsureSharedMemory(enabled ? m_volumePercent.load() : 100, enabled, m_superBoost.load(), m_boostMethod.load());
    LeaveCriticalSection(&m_cs);
}

void HookController::SetSuperBoost(bool superBoost) {
    m_superBoost = superBoost;

    EnterCriticalSection(&m_cs);
    if (m_pManager && m_pfnSetSuperBoost) {
        m_pfnSetSuperBoost(m_pManager, (void*)(uintptr_t)(superBoost ? 1 : 0));
    }
    EnsureSharedMemory(m_volumePercent.load(), m_enabled.load(), superBoost, m_boostMethod.load());
    LeaveCriticalSection(&m_cs);
}

void HookController::SetBoostMethod(int boostMethod) {
    m_boostMethod = boostMethod;

    EnterCriticalSection(&m_cs);
    if (m_pManager && m_pfnSetBoostMethod) {
        m_pfnSetBoostMethod(m_pManager, boostMethod);
    }
    EnsureSharedMemory(m_volumePercent.load(), m_enabled.load(), m_superBoost.load(), boostMethod);
    LeaveCriticalSection(&m_cs);
}
