#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <atomic>
#include "DspEngine.h"
#include "ApoController.h"
#include "HookController.h"

class AudioEngineNotificationClient;

class AudioEngine {
public:
    AudioEngine(DspEngine& dsp, ApoController& apo);
    ~AudioEngine();

    bool Start(HWND hWndMaster = NULL);
    void Stop();

    void OnDefaultDeviceChanged();
    void UpdateVolume(int volumePercent);
    void SetEnabled(bool enabled);
    void SetSuperBoost(bool enabled);
    void SetBoostMethod(int boostMethod);

    bool IsRunning() const { return m_running.load(); }
    ApoController& GetApoController() { return m_apo; }
    HookController& GetHookController() { return m_hook; }

private:
    DspEngine& m_dsp;
    ApoController& m_apo;
    HookController m_hook;
    std::atomic<bool> m_running;
    std::atomic<int> m_volumePercent;
    std::atomic<bool> m_enabled;
    std::atomic<bool> m_superBoost;
    std::atomic<int> m_boostMethod;

    void ApplySettings();

    IMMDeviceEnumerator* m_pEnumerator;
    AudioEngineNotificationClient* m_pNotificationClient;
};
