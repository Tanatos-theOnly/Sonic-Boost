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
      m_pEnumerator(nullptr),
      m_pNotificationClient(nullptr)
{
}

AudioEngine::~AudioEngine() {
    Stop();
}

bool AudioEngine::Start() {
    // 1. Initialize Injection Hook Engine (SBH64) for in-application audio interception
    m_hook.Initialize();

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
    m_hook.SetVolume(m_hook.GetVolume());
    m_apo.SetVolume(m_apo.GetVolume());
}

void AudioEngine::UpdateVolume(int volumePercent) {
    m_dsp.SetVolume(volumePercent);
    m_apo.SetVolume(volumePercent);
    m_hook.SetVolume(volumePercent);
}

void AudioEngine::SetEnabled(bool enabled) {
    m_dsp.SetEnabled(enabled);
    m_apo.SetEnabled(enabled);
    m_hook.SetEnabled(enabled);
}

void AudioEngine::SetSuperBoost(bool enabled) {
    m_dsp.SetSuperBoost(enabled);
    m_apo.SetSuperBoost(enabled);
    m_hook.SetSuperBoost(enabled);
}
