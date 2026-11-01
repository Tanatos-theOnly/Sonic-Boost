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
    int GetVolume() const { return m_volumePercent.load(); }
    bool IsEnabled() const { return m_enabled.load(); }
    bool IsSuperBoost() const { return m_superBoost.load(); }

private:
    bool TryConnect();
    void UpdateSharedData();
    static DWORD WINAPI WatcherThreadProc(LPVOID lpParam);
    void WatcherLoop();

    HANDLE m_hMapFile;
    void* m_pSharedBuf;
    HANDLE m_hUpdateEvent;
    HANDLE m_hCreateEvent;
    HANDLE m_hStopEvent;
    HANDLE m_hWatcherThread;

    std::atomic<int> m_volumePercent;
    std::atomic<bool> m_enabled;
    std::atomic<bool> m_superBoost;

    CRITICAL_SECTION m_cs;
};
