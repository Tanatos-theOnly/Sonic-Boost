#pragma once
#include <windows.h>
#include <sddl.h>
#include <string>
#include <atomic>

class ApoController {
public:
    ApoController();
    ~ApoController();

    bool Initialize();
    void Shutdown();

    void SetVolume(int volumePercent);
    void SetEnabled(bool enabled);
    void SetSuperBoost(bool superBoost);

    bool IsConnected() const { return m_hMapFile != NULL && m_pSharedBuf != nullptr; }
    int GetVolume() const { return m_volumePercent; }
    bool IsEnabled() const { return m_enabled; }
    bool IsSuperBoost() const { return m_superBoost; }

private:
    bool EnablePrivilege(LPCWSTR privilegeName);
    bool SetupSecurityAttributes(SECURITY_ATTRIBUTES& sa, PSECURITY_DESCRIPTOR& pSD);
    void UpdateSharedData();

    HANDLE m_hMapFile;
    void* m_pSharedBuf;
    HANDLE m_hUpdateEvent;
    HANDLE m_hCreateEvent;

    std::atomic<int> m_volumePercent;
    std::atomic<bool> m_enabled;
    std::atomic<bool> m_superBoost;

    CRITICAL_SECTION m_cs;
};
