#include "SoundBoosterApo.h"
#include <cstdio>
#include <sddl.h>

static void LogApo(const char* msg) {
    char buf[512];
    SYSTEMTIME st;
    GetLocalTime(&st);
    sprintf_s(buf, "[%02d:%02d:%02d.%03d] [PID %lu] [APO] %s\n",
              st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, GetCurrentProcessId(), msg);
    OutputDebugStringA(buf);
    FILE* f = fopen("C:\\Users\\Public\\soundbooster_apo.log", "a");
    if (f) {
        fputs(buf, f);
        fflush(f);
        fclose(f);
    }
}

static const struct {
    APO_REG_PROPERTIES props;
    IID extraInterfaces[1];
} g_ApoRegProps = {
    {
        CLSID_SoundBoosterApo,
        (APO_FLAG)0x0000000d,
        L"SonicBoost Global Audio Effect",
        L"Developed by Reddy A. Rohan on earth",
        1, 0,
        1, 1,
        1, 1,
        0xFFFFFFFF,  // u32MaxInstances: allow unlimited APO instances
        1,           // u32NumAPOInterfaces: 1 interface in list
        { __uuidof(IAudioProcessingObject) }
    },
    {}
};

const APO_REG_PROPERTIES* GetApoRegProperties() {
    return (const APO_REG_PROPERTIES*)&g_ApoRegProps;
}

CSoundBoosterAPO::CSoundBoosterAPO(IUnknown* pUnkOuter)
    : CBaseAudioProcessingObject((const APO_REG_PROPERTIES*)&g_ApoRegProps),
      m_innerUnknown(this),
      m_pUnkOuter(pUnkOuter),
      m_cRef(1),
      m_hFile(NULL),
      m_hMapFile(NULL),
      m_pSharedBuf(nullptr),
      m_hUpdateEvent(NULL),
      m_hCreateEvent(NULL),
      m_gainFactor(1.0f),
      m_boostEnabled(false),
      m_superBoostEnabled(true),
      m_loggedFramesInStream(0)
{
    m_u32SamplesPerFrame = 2;
    LogApo("CSoundBoosterAPO constructor called");
    CheckSharedConfig();
    LogApo("CSoundBoosterAPO constructor finished");
}

CSoundBoosterAPO::~CSoundBoosterAPO() {
    LogApo("CSoundBoosterAPO destructor called");
    if (m_pSharedBuf) {
        UnmapViewOfFile(m_pSharedBuf);
        m_pSharedBuf = nullptr;
    }
    if (m_hMapFile) {
        CloseHandle(m_hMapFile);
        m_hMapFile = NULL;
    }
    if (m_hFile) {
        CloseHandle(m_hFile);
        m_hFile = NULL;
    }
    if (m_hUpdateEvent) {
        CloseHandle(m_hUpdateEvent);
        m_hUpdateEvent = NULL;
    }
    if (m_hCreateEvent) {
        CloseHandle(m_hCreateEvent);
        m_hCreateEvent = NULL;
    }
}

// Delegating IUnknown
STDMETHODIMP CSoundBoosterAPO::QueryInterface(REFIID riid, void** ppvObject) {
    LPOLESTR pStr = NULL;
    StringFromIID(riid, &pStr);
    char buf[256];
    sprintf_s(buf, "Delegating QueryInterface: %ls (m_pUnkOuter=%p)", pStr ? pStr : L"null", m_pUnkOuter);
    if (pStr) CoTaskMemFree(pStr);
    LogApo(buf);

    if (m_pUnkOuter) {
        return m_pUnkOuter->QueryInterface(riid, ppvObject);
    }
    return NonDelegatingQueryInterface(riid, ppvObject);
}

STDMETHODIMP_(ULONG) CSoundBoosterAPO::AddRef() {
    if (m_pUnkOuter) {
        return m_pUnkOuter->AddRef();
    }
    return NonDelegatingAddRef();
}

STDMETHODIMP_(ULONG) CSoundBoosterAPO::Release() {
    if (m_pUnkOuter) {
        return m_pUnkOuter->Release();
    }
    return NonDelegatingRelease();
}

// Non-delegating IUnknown (used for aggregation)
STDMETHODIMP CSoundBoosterAPO::NonDelegatingQueryInterface(REFIID riid, void** ppvObject) {
    if (!ppvObject) return E_POINTER;
    *ppvObject = nullptr;

    LPOLESTR pStr = NULL;
    StringFromIID(riid, &pStr);
    char buf[256];
    sprintf_s(buf, "NonDelegatingQueryInterface: %ls", pStr ? pStr : L"null");
    if (pStr) CoTaskMemFree(pStr);
    LogApo(buf);

    if (riid == __uuidof(IUnknown)) {
        *ppvObject = static_cast<IUnknown*>(&m_innerUnknown);
    } else if (riid == __uuidof(IAudioProcessingObject)) {
        *ppvObject = static_cast<IAudioProcessingObject*>(this);
    } else if (riid == __uuidof(IAudioProcessingObjectRT)) {
        *ppvObject = static_cast<IAudioProcessingObjectRT*>(this);
    } else if (riid == __uuidof(IAudioProcessingObjectConfiguration)) {
        *ppvObject = static_cast<IAudioProcessingObjectConfiguration*>(this);
    } else if (riid == __uuidof(IAudioSystemEffects)) {
        *ppvObject = static_cast<IAudioSystemEffects*>(this);
    } else {
        LogApo("NonDelegatingQueryInterface: E_NOINTERFACE");
        return E_NOINTERFACE;
    }
    reinterpret_cast<IUnknown*>(*ppvObject)->AddRef();
    LogApo("NonDelegatingQueryInterface: S_OK");
    return S_OK;
}

STDMETHODIMP_(ULONG) CSoundBoosterAPO::NonDelegatingAddRef() {
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CSoundBoosterAPO::NonDelegatingRelease() {
    ULONG ulRef = InterlockedDecrement(&m_cRef);
    if (ulRef == 0) {
        delete this;
    }
    return ulRef;
}

// IAudioProcessingObject overrides with logging
STDMETHODIMP CSoundBoosterAPO::Reset() {
    LogApo("Reset called");
    return CBaseAudioProcessingObject::Reset();
}

STDMETHODIMP CSoundBoosterAPO::GetLatency(HNSTIME* pTime) {
    LogApo("GetLatency called");
    return CBaseAudioProcessingObject::GetLatency(pTime);
}

STDMETHODIMP CSoundBoosterAPO::GetRegistrationProperties(APO_REG_PROPERTIES** ppRegProps) {
    LogApo("GetRegistrationProperties called");
    HRESULT hr = CBaseAudioProcessingObject::GetRegistrationProperties(ppRegProps);
    char buf[128];
    sprintf_s(buf, "GetRegistrationProperties returned 0x%08X", hr);
    LogApo(buf);
    return hr;
}

STDMETHODIMP CSoundBoosterAPO::Initialize(UINT32 cbDataSize, BYTE* pbyData) {
    char buf[256];
    sprintf_s(buf, "Initialize called: cbDataSize=%u, pbyData=%p", cbDataSize, pbyData);
    LogApo(buf);

    HRESULT hr = CBaseAudioProcessingObject::Initialize(cbDataSize, pbyData);
    sprintf_s(buf, "CBaseAudioProcessingObject::Initialize returned 0x%08X", hr);
    LogApo(buf);
    return hr;
}

STDMETHODIMP CSoundBoosterAPO::IsInputFormatSupported(
    IAudioMediaType* pOutputFormat,
    IAudioMediaType* pRequestedInputFormat,
    IAudioMediaType** ppSupportedInputFormat)
{
    char buf[256];
    sprintf_s(buf, "IsInputFormatSupported called: pOut=%p, pReqIn=%p, ppSuppIn=%p",
              pOutputFormat, pRequestedInputFormat, ppSupportedInputFormat);
    LogApo(buf);

    HRESULT hr = CBaseAudioProcessingObject::IsInputFormatSupported(
        pOutputFormat, pRequestedInputFormat, ppSupportedInputFormat);

    sprintf_s(buf, "CBaseAudioProcessingObject::IsInputFormatSupported returned 0x%08X", hr);
    LogApo(buf);
    return hr;
}

STDMETHODIMP CSoundBoosterAPO::IsOutputFormatSupported(
    IAudioMediaType* pInputFormat,
    IAudioMediaType* pRequestedOutputFormat,
    IAudioMediaType** ppSupportedOutputFormat)
{
    char buf[256];
    sprintf_s(buf, "IsOutputFormatSupported called: pIn=%p, pReqOut=%p, ppSuppOut=%p",
              pInputFormat, pRequestedOutputFormat, ppSupportedOutputFormat);
    LogApo(buf);

    HRESULT hr = CBaseAudioProcessingObject::IsOutputFormatSupported(
        pInputFormat, pRequestedOutputFormat, ppSupportedOutputFormat);

    sprintf_s(buf, "CBaseAudioProcessingObject::IsOutputFormatSupported returned 0x%08X", hr);
    LogApo(buf);
    return hr;
}

STDMETHODIMP CSoundBoosterAPO::GetInputChannelCount(UINT32* pu32ChannelCount) {
    LogApo("GetInputChannelCount called");
    HRESULT hr = CBaseAudioProcessingObject::GetInputChannelCount(pu32ChannelCount);
    char buf[128];
    sprintf_s(buf, "GetInputChannelCount returned 0x%08X (count=%u)", hr, pu32ChannelCount ? *pu32ChannelCount : 0);
    LogApo(buf);
    return hr;
}

STDMETHODIMP CSoundBoosterAPO::LockForProcess(
    UINT32 u32NumInputConnections,
    APO_CONNECTION_DESCRIPTOR** ppInputConnections,
    UINT32 u32NumOutputConnections,
    APO_CONNECTION_DESCRIPTOR** ppOutputConnections)
{
    LogApo("LockForProcess called");
    HRESULT hr = CBaseAudioProcessingObject::LockForProcess(
        u32NumInputConnections, ppInputConnections,
        u32NumOutputConnections, ppOutputConnections);

    char buf[128];
    sprintf_s(buf, "CBaseAudioProcessingObject::LockForProcess returned 0x%08X", hr);
    LogApo(buf);

    if (ppInputConnections && u32NumInputConnections > 0 && ppInputConnections[0] && ppInputConnections[0]->pFormat) {
        UNCOMPRESSEDAUDIOFORMAT uncompressed = {};
        if (SUCCEEDED(ppInputConnections[0]->pFormat->GetUncompressedAudioFormat(&uncompressed))) {
            m_u32SamplesPerFrame = uncompressed.dwSamplesPerFrame;
        }
    }
    if (m_u32SamplesPerFrame == 0) {
        m_u32SamplesPerFrame = 2;
    }

    CheckSharedConfig();
    m_loggedFramesInStream = 0;
    char cfgBuf[256];
    sprintf_s(cfgBuf, "LockForProcess completed: boostEn=%d, gain=%.3f, raw='%.32s'",
              (int)m_boostEnabled, m_gainFactor, m_pSharedBuf ? m_pSharedBuf : "null");
    LogApo(cfgBuf);
    return hr;
}

STDMETHODIMP CSoundBoosterAPO::UnlockForProcess() {
    LogApo("UnlockForProcess called");
    return CBaseAudioProcessingObject::UnlockForProcess();
}

static bool ReadStateFile(char* outBuf, size_t maxLen) {
    FILE* f = fopen("C:\\Users\\Public\\sbapo_state.txt", "r");
    if (!f) return false;
    size_t bytes = fread(outBuf, 1, maxLen - 1, f);
    fclose(f);
    if (bytes > 0) {
        outBuf[bytes] = '\0';
        return true;
    }
    return false;
}

void CSoundBoosterAPO::CheckSharedConfig() {
    if (!m_pSharedBuf) {
        if (!m_hFile) {
            m_hFile = CreateFileA(
                "C:\\Users\\Public\\sbapo_state.dat",
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );
            if (m_hFile == INVALID_HANDLE_VALUE) {
                m_hFile = CreateFileA(
                    "C:\\Users\\Public\\sbapo_state.dat",
                    GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL
                );
            }
            if (m_hFile == INVALID_HANDLE_VALUE) {
                m_hFile = NULL;
            } else {
                LARGE_INTEGER sz = {};
                GetFileSizeEx(m_hFile, &sz);
                if (sz.QuadPart < 256) {
                    sz.QuadPart = 256;
                    SetFilePointerEx(m_hFile, sz, NULL, FILE_BEGIN);
                    SetEndOfFile(m_hFile);
                }
            }
        }

        if (m_hFile && !m_hMapFile) {
            m_hMapFile = CreateFileMappingA(m_hFile, NULL, PAGE_READWRITE, 0, 256, NULL);
            if (!m_hMapFile) {
                m_hMapFile = CreateFileMappingA(m_hFile, NULL, PAGE_READONLY, 0, 256, NULL);
            }
        }

        if (m_hMapFile) {
            m_pSharedBuf = (const char*)MapViewOfFile(m_hMapFile, FILE_MAP_READ, 0, 0, 256);
            if (m_pSharedBuf) {
                LogApo("Successfully mapped sbapo_state.dat shared bridge in audiodg");
            }
        }
    }

    const char* pConfig = m_pSharedBuf;
    char fallbackState[256] = {};
    if (!pConfig || pConfig[0] == '\0') {
        if (ReadStateFile(fallbackState, sizeof(fallbackState))) {
            pConfig = fallbackState;
        }
    }

    if (pConfig) {
        // Parse config text
        if (pConfig[0] == '#') {
            m_boostEnabled = false;
            m_gainFactor = 1.0f;
        } else {
            int boostDb = 0;
            int mode = 1;
            if (sscanf_s(pConfig, "Boost: %d dB %d", &boostDb, &mode) >= 1) {
                m_superBoostEnabled = (mode != 0);
                if (boostDb <= 0) {
                    m_boostEnabled = false;
                    m_gainFactor = 1.0f;
                } else {
                    m_boostEnabled = true;
                    // Correct dB-to-linear conversion: e.g. 14 dB -> powf(10, 14/20) = 5.012x
                    m_gainFactor = powf(10.0f, (float)boostDb / 20.0f);
                }
            }
        }
    }
}

inline float CSoundBoosterAPO::ApplySuperBoostLimit(float sample) {
    if (sample > 1.0f) {
        float excess = sample - 1.0f;
        return 1.0f + tanhf(excess * 0.5f) * 0.5f;
    } else if (sample < -1.0f) {
        float excess = sample + 1.0f;
        return -1.0f + tanhf(excess * 0.5f) * 0.5f;
    }
    return sample;
}

#pragma AVRT_CODE_BEGIN
STDMETHODIMP_(void) CSoundBoosterAPO::APOProcess(
    UINT32 u32NumInputConnections,
    APO_CONNECTION_PROPERTY** ppInputConnections,
    UINT32 u32NumOutputConnections,
    APO_CONNECTION_PROPERTY** ppOutputConnections)
{
    if (u32NumInputConnections == 0 || !ppInputConnections ||
        u32NumOutputConnections == 0 || !ppOutputConnections) {
        return;
    }

    APO_CONNECTION_PROPERTY* pInConn = ppInputConnections[0];
    APO_CONNECTION_PROPERTY* pOutConn = ppOutputConnections[0];

    if (!pInConn || !pOutConn) return;

    UINT32 validFrames = pInConn->u32ValidFrameCount;
    pOutConn->u32ValidFrameCount = validFrames;
    pOutConn->u32BufferFlags = pInConn->u32BufferFlags;

    if (validFrames == 0 || (pInConn->u32BufferFlags & BUFFER_SILENT)) {
        return;
    }

    // Refresh configuration periodically or when signaled
    CheckSharedConfig();

    float* pIn = reinterpret_cast<float*>(pInConn->pBuffer);
    float* pOut = reinterpret_cast<float*>(pOutConn->pBuffer);

    if (!pIn || !pOut) return;

    UINT32 channels = (m_u32SamplesPerFrame > 0) ? m_u32SamplesPerFrame : 2;
    UINT32 numSamples = validFrames * channels;

    static float s_lastLoggedGain = -1.0f;
    static bool s_lastLoggedBoost = false;
    if (m_gainFactor != s_lastLoggedGain || m_boostEnabled != s_lastLoggedBoost) {
        s_lastLoggedGain = m_gainFactor;
        s_lastLoggedBoost = m_boostEnabled;
        char dbg[256];
        sprintf_s(dbg, "APOProcess [GAIN STATE]: gain=%.3f, boostEn=%d, in[0]=%.4f, raw='%.32s'",
                  m_gainFactor, (int)m_boostEnabled, pIn[0], m_pSharedBuf ? m_pSharedBuf : "null");
        LogApo(dbg);
    }

    if (!m_boostEnabled || m_gainFactor <= 1.001f) {
        if (pIn != pOut) {
            memcpy(pOut, pIn, numSamples * sizeof(float));
        }
        return;
    }

    float gain = m_gainFactor;
    bool superBoost = m_superBoostEnabled;

    for (UINT32 i = 0; i < numSamples; ++i) {
        float s = pIn[i] * gain;
        if (superBoost) {
            s = ApplySuperBoostLimit(s);
        } else {
            if (s > 1.0f) s = 1.0f;
            else if (s < -1.0f) s = -1.0f;
        }
        pOut[i] = s;
    }
}
#pragma AVRT_CODE_END
