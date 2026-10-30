#pragma once
#include <windows.h>
#include <AudioEngineBaseAPO.h>
#include <BaseAudioProcessingObject.h>
#include <cmath>

// CLSID matching reference Sbapo Global Effect for 100% binary drop-in compatibility
// {BABAE75F-A7AA-4A13-B1F7-BB2716FD003E}
static const GUID CLSID_SoundBoosterApo = 
    { 0xbabae75f, 0xa7aa, 0x4a13, { 0xb1, 0xf7, 0xbb, 0x27, 0x16, 0xfd, 0x00, 0x3e } };

class __declspec(novtable) CSoundBoosterAPO :
    public CBaseAudioProcessingObject,
    public IAudioSystemEffects
{
public:
    CSoundBoosterAPO();
    virtual ~CSoundBoosterAPO();

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IAudioProcessingObjectRT
    STDMETHOD_(void, APOProcess)(
        UINT32 u32NumInputConnections,
        APO_CONNECTION_PROPERTY** ppInputConnections,
        UINT32 u32NumOutputConnections,
        APO_CONNECTION_PROPERTY** ppOutputConnections
    ) override;

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

    LONG m_cRef;
    HANDLE m_hMapFile;
    const char* m_pSharedBuf;
    HANDLE m_hUpdateEvent;

    float m_gainFactor;
    bool m_boostEnabled;
    bool m_superBoostEnabled;
};
