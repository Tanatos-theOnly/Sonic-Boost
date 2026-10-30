#include "HookController.h"
#include <cstdio>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

HookController::HookController()
    : m_hModule(NULL),
      m_pManager(nullptr),
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
      m_volumePercent(100),
      m_enabled(true),
      m_superBoost(true)
{
    InitializeCriticalSection(&m_cs);
}

HookController::~HookController() {
    Shutdown();
    DeleteCriticalSection(&m_cs);
}

void HookController::EnsureSharedMemory(int volumePercent, bool enabled, bool superBoost) {
    HANDLE hMap = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"CKLHookShared");
    if (!hMap) {
        hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0x1000, L"CKLHookShared");
    }
    if (hMap) {
        void* pBuf = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0x100);
        if (pBuf) {
            DWORD* pDwords = (DWORD*)pBuf;
            pDwords[0x18 / 4] = enabled ? 1 : 0;
            pDwords[0xA0 / 4] = (DWORD)volumePercent;
            pDwords[0xA4 / 4] = GetCurrentProcessId();
            pDwords[0xA8 / 4] = superBoost ? 1 : 0;
            UnmapViewOfFile(pBuf);
        }
        CloseHandle(hMap);
    }
}

bool HookController::Initialize() {
    EnterCriticalSection(&m_cs);

    if (m_pManager) {
        LeaveCriticalSection(&m_cs);
        return true;
    }

    WCHAR szModule[MAX_PATH];
    GetModuleFileNameW(NULL, szModule, MAX_PATH);
    WCHAR* pSlash = wcsrchr(szModule, L'\\');
    if (pSlash) *pSlash = L'\0';

    std::wstring localDll = std::wstring(szModule) + L"\\SBH64.dll";
    m_hModule = LoadLibraryW(localDll.c_str());

    if (!m_hModule) {
        m_hModule = LoadLibraryW(L"D:\\Tools\\Softwares\\SBH64.dll");
    }

    if (!m_hModule) {
        LeaveCriticalSection(&m_cs);
        return false;
    }

    PFN_KLHGetManager pfnGetMgr = (PFN_KLHGetManager)GetProcAddress(m_hModule, "KLHGetManager");
    if (!pfnGetMgr) {
        FreeLibrary(m_hModule);
        m_hModule = NULL;
        LeaveCriticalSection(&m_cs);
        return false;
    }

    m_pManager = pfnGetMgr();
    if (!m_pManager) {
        FreeLibrary(m_hModule);
        m_hModule = NULL;
        LeaveCriticalSection(&m_cs);
        return false;
    }

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

    // Initialize controller window and IPC
    if (m_pfnInitWindow) {
        m_pfnInitWindow(m_pManager, NULL);
    }

    // Install the global hook across all processes
    if (m_pfnHookInstall) {
        m_pfnHookInstall(m_pManager);
    }

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

    // Synchronize shared memory bridge
    EnsureSharedMemory(m_volumePercent.load(), m_enabled.load(), m_superBoost.load());

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

    EnsureSharedMemory(100, false, false);

    if (m_hModule) {
        FreeLibrary(m_hModule);
        m_hModule = NULL;
    }

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
    EnsureSharedMemory(volumePercent, m_enabled.load(), m_superBoost.load());
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
    EnsureSharedMemory(enabled ? m_volumePercent.load() : 100, enabled, m_superBoost.load());
    LeaveCriticalSection(&m_cs);
}

void HookController::SetSuperBoost(bool superBoost) {
    m_superBoost = superBoost;

    EnterCriticalSection(&m_cs);
    if (m_pManager && m_pfnSetSuperBoost) {
        m_pfnSetSuperBoost(m_pManager, (void*)(uintptr_t)(superBoost ? 1 : 0));
    }
    EnsureSharedMemory(m_volumePercent.load(), m_enabled.load(), superBoost);
    LeaveCriticalSection(&m_cs);
}
