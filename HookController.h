#pragma once
#include <windows.h>
#include <atomic>
#include <string>

// Robust, direct function pointer bindings for SBH64.dll vtable
typedef void (__fastcall *PFN_HookUninstall)(void* pThis);
typedef bool (__fastcall *PFN_HookInstall)(void* pThis, HWND hWnd);
typedef void (__fastcall *PFN_SetEnabled)(void* pThis, bool enabled);
typedef int  (__fastcall *PFN_GetVolume)(void* pThis);
typedef void (__fastcall *PFN_SetVolume)(void* pThis, int volumePercent);
typedef void (__fastcall *PFN_SetSuperBoost)(void* pThis, void* param);

typedef void* (__cdecl *PFN_KLHGetManager)();

class HookController {
public:
    HookController();
    ~HookController();

    bool Initialize();
    void Shutdown();

    void SetVolume(int volumePercent);
    void SetEnabled(bool enabled);
    void SetSuperBoost(bool superBoost);

    bool IsActive() const { return m_pManager != nullptr; }
    int GetVolume() const { return m_volumePercent.load(); }

private:
    HMODULE m_hModule;
    void* m_pManager;

    PFN_HookUninstall  m_pfnHookUninstall;
    PFN_HookInstall    m_pfnHookInstall;
    PFN_SetEnabled      m_pfnSetEnabled;
    PFN_GetVolume      m_pfnGetVolume;
    PFN_SetVolume      m_pfnSetVolume;
    PFN_SetSuperBoost  m_pfnSetSuperBoost;

    std::atomic<int> m_volumePercent;
    std::atomic<bool> m_enabled;
    std::atomic<bool> m_superBoost;
    CRITICAL_SECTION m_cs;
};
