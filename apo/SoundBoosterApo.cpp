#include "SoundBoosterApo.h"
#include <cstdio>

#pragma AVRT_CONST_BEGIN
static const struct {
    APO_REG_PROPERTIES props;
    IID extraInterfaces[1];
} g_ApoRegProps = {
    {
        CLSID_SoundBoosterApo,
        (APO_FLAG)(APO_FLAG_DEFAULT | APO_FLAG_INPLACE | APO_FLAG_SAMPLESPERFRAME_MUST_MATCH | APO_FLAG_FRAMESPERSECOND_MUST_MATCH | APO_FLAG_BITSPERSAMPLE_MUST_MATCH),
        L"SonicBoost Global Audio Effect",
        L"Copyright (c) Reddy A. Rohan. All rights reserved.",
        1, 0,
        1, 1,
        1, 1,
        1,
        1,
        { __uuidof(IAudioProcessingObjectRT) }
    },
    {}
};
#pragma AVRT_CONST_END

CSoundBoosterAPO::CSoundBoosterAPO()
    : CBaseAudioProcessingObject((const APO_REG_PROPERTIES*)&g_ApoRegProps),
      m_cRef(1),
      m_hMapFile(NULL),
      m_pSharedBuf(nullptr),
      m_hUpdateEvent(NULL),
      m_gainFactor(1.0f),
      m_boostEnabled(false),
      m_superBoostEnabled(true)
{
    m_u32SamplesPerFrame = 2;
}

CSoundBoosterAPO::~CSoundBoosterAPO() {
    if (m_pSharedBuf) {
        UnmapViewOfFile(m_pSharedBuf);
        m_pSharedBuf = nullptr;
    }
    if (m_hMapFile) {
        CloseHandle(m_hMapFile);
        m_hMapFile = NULL;
    }
    if (m_hUpdateEvent) {
        CloseHandle(m_hUpdateEvent);
        m_hUpdateEvent = NULL;
    }
}

STDMETHODIMP CSoundBoosterAPO::QueryInterface(REFIID riid, void** ppvObject) {
    if (!ppvObject) return E_POINTER;

    if (riid == __uuidof(IUnknown) ||
        riid == __uuidof(IAudioProcessingObject) ||
        riid == __uuidof(IAudioProcessingObjectRT) ||
        riid == __uuidof(IAudioProcessingObjectConfiguration)) {
        *ppvObject = static_cast<IAudioProcessingObjectRT*>(this);
        AddRef();
        return S_OK;
    }

    if (riid == __uuidof(IAudioSystemEffects)) {
        *ppvObject = static_cast<IAudioSystemEffects*>(this);
        AddRef();
        return S_OK;
    }

    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CSoundBoosterAPO::AddRef() {
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) CSoundBoosterAPO::Release() {
    ULONG ulRef = InterlockedDecrement(&m_cRef);
    if (ulRef == 0) {
        delete this;
    }
    return ulRef;
}

STDMETHODIMP CSoundBoosterAPO::LockForProcess(
    UINT32 u32NumInputConnections,
    APO_CONNECTION_DESCRIPTOR** ppInputConnections,
    UINT32 u32NumOutputConnections,
    APO_CONNECTION_DESCRIPTOR** ppOutputConnections)
{
    HRESULT hr = CBaseAudioProcessingObject::LockForProcess(
        u32NumInputConnections, ppInputConnections,
        u32NumOutputConnections, ppOutputConnections);

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
    return hr;
}

STDMETHODIMP CSoundBoosterAPO::UnlockForProcess() {
    return CBaseAudioProcessingObject::UnlockForProcess();
}

void CSoundBoosterAPO::CheckSharedConfig() {
    if (!m_hMapFile) {
        m_hMapFile = OpenFileMappingA(FILE_MAP_READ, FALSE, "Global\\SbapoConfigData");
        if (m_hMapFile) {
            m_pSharedBuf = (const char*)MapViewOfFile(m_hMapFile, FILE_MAP_READ, 0, 0, 256);
        }
    }
    if (!m_hUpdateEvent) {
        m_hUpdateEvent = OpenEventA(SYNCHRONIZE, FALSE, "Global\\SbapoDataUpdate");
    }

    if (m_pSharedBuf) {
        // Parse config text
        if (m_pSharedBuf[0] == '#') {
            m_boostEnabled = false;
            m_gainFactor = 1.0f;
        } else {
            int boostDb = 0;
            int mode = 1;
            if (sscanf_s(m_pSharedBuf, "Boost: %d dB %d", &boostDb, &mode) >= 1) {
                if (boostDb <= 0) {
                    m_boostEnabled = false;
                    m_gainFactor = 1.0f;
                } else {
                    m_boostEnabled = true;
                    // Linear mapping: boostDb 0..200 -> Linear Gain 1.0f .. 5.0f
                    m_gainFactor = 1.0f + ((float)boostDb / 200.0f) * 4.0f;
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
