#pragma once
#include <windows.h>
#include <AudioEngineBaseAPO.h>
#include <BaseAudioProcessingObject.h>
#include <cmath>

// CLSID matching reference Sbapo Global Effect for 100% binary drop-in compatibility
// {BABAE75F-A7AA-4A13-B1F7-BB2716FD003E}
static const GUID CLSID_SoundBoosterApo = 
    { 0xbabae75f, 0xa7aa, 0x4a13, { 0xb1, 0xf7, 0xbb, 0x27, 0x16, 0xfd, 0x00, 0x3e } };

const APO_REG_PROPERTIES* GetApoRegProperties();

class CSoundBoosterAPO :
    public CBaseAudioProcessingObject,
    public IAudioSystemEffects
{
public:
    CSoundBoosterAPO(IUnknown* pUnkOuter = nullptr);
    virtual ~CSoundBoosterAPO();

    // Non-delegating IUnknown methods (used for aggregation)
    STDMETHOD(NonDelegatingQueryInterface)(REFIID riid, void** ppvObject);
    STDMETHOD_(ULONG, NonDelegatingAddRef)();
    STDMETHOD_(ULONG, NonDelegatingRelease)();

    // Delegating IUnknown methods (delegates to m_pUnkOuter if aggregated, otherwise non-delegating)
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IAudioProcessingObject
    STDMETHOD(Reset)(void) override;
    STDMETHOD(GetLatency)(_Out_ HNSTIME* pTime) override;
    STDMETHOD(GetRegistrationProperties)(_Outptr_opt_ APO_REG_PROPERTIES** ppRegProps) override;
    STDMETHOD(Initialize)(_In_ UINT32 cbDataSize, _In_reads_bytes_(cbDataSize) BYTE* pbyData) override;
    STDMETHOD(IsInputFormatSupported)(_In_opt_ IAudioMediaType* pOutputFormat, _In_opt_ IAudioMediaType* pRequestedInputFormat, _Outptr_opt_ IAudioMediaType** ppSupportedInputFormat) override;
    STDMETHOD(IsOutputFormatSupported)(_In_opt_ IAudioMediaType* pInputFormat, _In_opt_ IAudioMediaType* pRequestedOutputFormat, _Outptr_opt_ IAudioMediaType** ppSupportedOutputFormat) override;
    STDMETHOD(GetInputChannelCount)(_Out_ UINT32* pu32ChannelCount) override;

    // IAudioProcessingObjectRT
    STDMETHOD_(void, APOProcess)(
        UINT32 u32NumInputConnections,
        APO_CONNECTION_PROPERTY** ppInputConnections,
        UINT32 u32NumOutputConnections,
        APO_CONNECTION_PROPERTY** ppOutputConnections
    ) override;

    // IAudioProcessingObjectConfiguration
    STDMETHOD(LockForProcess)(
        UINT32 u32NumInputConnections,
        APO_CONNECTION_DESCRIPTOR** ppInputConnections,
        UINT32 u32NumOutputConnections,
        APO_CONNECTION_DESCRIPTOR** ppOutputConnections
    ) override;

    STDMETHOD(UnlockForProcess)() override;

private:
    void CheckSharedConfig();
    inline float ApplySuperBoostLimit(float sample);

    class CInnerUnknown : public IUnknown {
    public:
        CSoundBoosterAPO* m_pParent;
        CInnerUnknown(CSoundBoosterAPO* pParent) : m_pParent(pParent) {}

        STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
            return m_pParent->NonDelegatingQueryInterface(riid, ppv);
        }
        STDMETHODIMP_(ULONG) AddRef() override {
            return m_pParent->NonDelegatingAddRef();
        }
        STDMETHODIMP_(ULONG) Release() override {
            return m_pParent->NonDelegatingRelease();
        }
    } m_innerUnknown;

    IUnknown* m_pUnkOuter;
    LONG m_cRef;
    HANDLE m_hFile;
    HANDLE m_hMapFile;
    const char* m_pSharedBuf;
    HANDLE m_hUpdateEvent;
    HANDLE m_hCreateEvent;

    float m_gainFactor;
    bool m_boostEnabled;
    bool m_superBoostEnabled;
    int m_loggedFramesInStream;
};
