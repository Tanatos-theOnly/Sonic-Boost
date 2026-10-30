#include "HookController.h"
#include <cstdio>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

HookController::HookController()
    : m_hModule(NULL),
      m_pManager(nullptr),
      m_pfnHookUninstall(nullptr),
      m_pfnHookInstall(nullptr),
      m_pfnSetEnabled(nullptr),
      m_pfnGetVolume(nullptr),
      m_pfnSetVolume(nullptr),
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

    // Resolve vtable function pointers directly
    void** vtbl = *(void***)m_pManager;
    m_pfnHookUninstall = (PFN_HookUninstall)vtbl[0];
    m_pfnHookInstall   = (PFN_HookInstall)vtbl[1];
    m_pfnSetEnabled    = (PFN_SetEnabled)vtbl[7];
    m_pfnGetVolume     = (PFN_GetVolume)vtbl[8];
    m_pfnSetVolume     = (PFN_SetVolume)vtbl[9];
    m_pfnSetSuperBoost = (PFN_SetSuperBoost)vtbl[11];

    // Install the global hook across all processes
    if (m_pfnHookInstall) {
        m_pfnHookInstall(m_pManager, NULL);
    }

    // Apply current settings
    if (m_pfnSetEnabled) {
        m_pfnSetEnabled(m_pManager, m_enabled.load());
    }
    if (m_pfnSetVolume) {
        m_pfnSetVolume(m_pManager, m_volumePercent.load());
    }
    if (m_pfnSetSuperBoost) {
        m_pfnSetSuperBoost(m_pManager, (void*)(uintptr_t)(m_superBoost.load() ? 1 : 0));
    }

    LeaveCriticalSection(&m_cs);
    return true;
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
        m_pManager = nullptr;
    }

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
    LeaveCriticalSection(&m_cs);
}

void HookController::SetSuperBoost(bool superBoost) {
    m_superBoost = superBoost;

    EnterCriticalSection(&m_cs);
    if (m_pManager && m_pfnSetSuperBoost) {
        m_pfnSetSuperBoost(m_pManager, (void*)(uintptr_t)(superBoost ? 1 : 0));
    }
    LeaveCriticalSection(&m_cs);
}
