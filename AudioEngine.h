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

    bool Start();
    void Stop();

    void OnDefaultDeviceChanged();
    void UpdateVolume(int volumePercent);
    void SetEnabled(bool enabled);
    void SetSuperBoost(bool enabled);

    ApoController& GetApoController() { return m_apo; }
    HookController& GetHookController() { return m_hook; }

private:
    DspEngine& m_dsp;
    ApoController& m_apo;
    HookController m_hook;
    std::atomic<bool> m_running;

    IMMDeviceEnumerator* m_pEnumerator;
    AudioEngineNotificationClient* m_pNotificationClient;
};
