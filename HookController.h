#pragma once
#include <windows.h>
#include <atomic>
#include <string>

// Direct function pointer bindings for SBH64.dll vtable
typedef bool  (__fastcall *PFN_InitWindow)(void* pThis, HWND hWnd);       // vtbl[1]
typedef void  (__fastcall *PFN_DestroyWindow)(void* pThis);              // vtbl[2]
typedef bool  (__fastcall *PFN_HookInstall)(void* pThis);                // vtbl[3]
typedef bool  (__fastcall *PFN_HookUninstall)(void* pThis);              // vtbl[4]
typedef bool  (__fastcall *PFN_IsHookInstalled)(void* pThis);            // vtbl[5]
typedef bool  (__fastcall *PFN_IsEnabled)(void* pThis);                  // vtbl[6]
typedef void  (__fastcall *PFN_SetEnabled)(void* pThis, bool enabled);   // vtbl[7]
typedef int   (__fastcall *PFN_GetVolume)(void* pThis);                  // vtbl[8]
typedef void  (__fastcall *PFN_SetVolume)(void* pThis, int volume);      // vtbl[9]
typedef void* (__fastcall *PFN_GetSuperBoost)(void* pThis);             // vtbl[10]
typedef void  (__fastcall *PFN_SetSuperBoost)(void* pThis, void* param); // vtbl[11]
typedef int   (__fastcall *PFN_GetBoostMethod)(void* pThis);             // vtbl[12]
typedef void  (__fastcall *PFN_SetBoostMethod)(void* pThis, int method); // vtbl[13]

typedef void* (__cdecl *PFN_KLHGetManager)();

class HookController {
public:
    HookController();
    ~HookController();

    bool Initialize(HWND hWndMaster = NULL);
    void Shutdown();

    void SetVolume(int volumePercent);
    void SetEnabled(bool enabled);
    void SetSuperBoost(bool superBoost);
    void SetBoostMethod(int boostMethod);

    bool IsActive() const { return m_pManager != nullptr; }
    int  GetVolume() const { return m_volumePercent.load(); }
    int  GetBoostMethod() const { return m_boostMethod.load(); }
    bool IsHookInstalled() const;

private:
    void EnsureSharedMemory(int volumePercent, bool enabled, bool superBoost, int boostMethod = 2);

    HMODULE m_hModule;
    void* m_pManager;
    HWND m_hWndMaster;

    PFN_InitWindow      m_pfnInitWindow;      // vtbl[1]
    PFN_DestroyWindow   m_pfnDestroyWindow;   // vtbl[2]
    PFN_HookInstall     m_pfnHookInstall;     // vtbl[3]
    PFN_HookUninstall   m_pfnHookUninstall;   // vtbl[4]
    PFN_IsHookInstalled m_pfnIsHookInstalled; // vtbl[5]
    PFN_IsEnabled       m_pfnIsEnabled;       // vtbl[6]
    PFN_SetEnabled      m_pfnSetEnabled;      // vtbl[7]
    PFN_GetVolume       m_pfnGetVolume;       // vtbl[8]
    PFN_SetVolume       m_pfnSetVolume;       // vtbl[9]
    PFN_GetSuperBoost   m_pfnGetSuperBoost;   // vtbl[10]
    PFN_SetSuperBoost   m_pfnSetSuperBoost;   // vtbl[11]
    PFN_GetBoostMethod  m_pfnGetBoostMethod;  // vtbl[12]
    PFN_SetBoostMethod  m_pfnSetBoostMethod;  // vtbl[13]

    std::atomic<int> m_volumePercent;
    std::atomic<bool> m_enabled;
    std::atomic<bool> m_superBoost;
    std::atomic<int> m_boostMethod;
    CRITICAL_SECTION m_cs;
};
