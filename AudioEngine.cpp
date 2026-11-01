#include "AudioEngine.h"

class AudioEngineNotificationClient : public IMMNotificationClient {
public:
    AudioEngineNotificationClient(AudioEngine* engine) : m_cRef(1), m_engine(engine) {}
    virtual ~AudioEngineNotificationClient() {}

    ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&m_cRef);
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG ulRef = InterlockedDecrement(&m_cRef);
        if (ulRef == 0) delete this;
        return ulRef;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvInterface) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
            AddRef();
            *ppvInterface = (IMMNotificationClient*)this;
            return S_OK;
        }
        *ppvInterface = NULL;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId) override {
        if (flow == eRender && (role == eConsole || role == eMultimedia)) {
            if (m_engine) {
                m_engine->OnDefaultDeviceChanged();
            }
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) override { return S_OK; }

private:
    LONG m_cRef;
    AudioEngine* m_engine;
};

AudioEngine::AudioEngine(DspEngine& dsp, ApoController& apo)
    : m_dsp(dsp),
      m_apo(apo),
      m_hook(),
      m_running(false),
      m_volumePercent(100),
      m_enabled(true),
      m_superBoost(false),
      m_boostMethod(3),
      m_pEnumerator(nullptr),
      m_pNotificationClient(nullptr)
{
}

AudioEngine::~AudioEngine() {
    Stop();
}

bool AudioEngine::Start(HWND hWndMaster) {
    // 1. Initialize Injection Hook Engine (SBH64) for in-application audio interception
    m_hook.Initialize(hWndMaster);

    // 2. Initialize Core Audio APO communication
    m_apo.Initialize();

    // 3. Register for default audio endpoint notifications
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), (void**)&m_pEnumerator);
    if (SUCCEEDED(hr) && m_pEnumerator) {
        m_pNotificationClient = new AudioEngineNotificationClient(this);
        m_pEnumerator->RegisterEndpointNotificationCallback(m_pNotificationClient);
    }

    m_running = true;
    ApplySettings();
    return true;
}

void AudioEngine::Stop() {
    if (!m_running) return;
    m_running = false;

    if (m_pEnumerator && m_pNotificationClient) {
        m_pEnumerator->UnregisterEndpointNotificationCallback(m_pNotificationClient);
        m_pNotificationClient->Release();
        m_pNotificationClient = nullptr;
    }
    if (m_pEnumerator) {
        m_pEnumerator->Release();
        m_pEnumerator = nullptr;
    }

    m_hook.Shutdown();
    m_apo.Shutdown();
}

void AudioEngine::OnDefaultDeviceChanged() {
    ApplySettings();
}

void AudioEngine::ApplySettings() {
    if (!m_running.load()) return;

    int method = m_boostMethod.load();
    int vol = m_volumePercent.load();
    bool en = m_enabled.load();
    bool super = m_superBoost.load();

    m_dsp.SetVolume(vol);
    m_dsp.SetEnabled(en);
    m_dsp.SetSuperBoost(super);

    if (method == 1) { // 1 = Injection only
        m_hook.SetBoostMethod(1);
        m_hook.SetEnabled(en);
        m_hook.SetVolume(vol);
        m_hook.SetSuperBoost(super);

        m_apo.SetEnabled(false);
        m_apo.SetVolume(100);
    } else if (method == 2) { // 2 = Injection and APO effect (Dual Engine)
        m_hook.SetBoostMethod(2);
        m_hook.SetEnabled(en);
        m_hook.SetVolume(vol);
        m_hook.SetSuperBoost(super);

        m_apo.SetEnabled(en);
        m_apo.SetVolume(vol);
        m_apo.SetSuperBoost(super);
    } else if (method == 3) { // 3 = APO effect only
        m_hook.SetEnabled(false);
        m_hook.SetVolume(100);

        m_apo.SetEnabled(en);
        m_apo.SetVolume(vol);
        m_apo.SetSuperBoost(super);
    }
}

void AudioEngine::UpdateVolume(int volumePercent) {
    m_volumePercent = volumePercent;
    ApplySettings();
}

void AudioEngine::SetEnabled(bool enabled) {
    m_enabled = enabled;
    ApplySettings();
}

void AudioEngine::SetSuperBoost(bool enabled) {
    m_superBoost = enabled;
    ApplySettings();
}

void AudioEngine::SetBoostMethod(int boostMethod) {
    m_boostMethod = boostMethod;
    ApplySettings();
}
