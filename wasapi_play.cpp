#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <cmath>
#include <cstdio>

int main() {
    CoInitialize(NULL);

    IMMDeviceEnumerator* pEnumerator = NULL;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) { printf("CoCreateInstance failed: 0x%08X\n", hr); return 1; }

    IMMDevice* pDevice = NULL;
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
    if (FAILED(hr)) { printf("GetDefaultAudioEndpoint failed: 0x%08X\n", hr); return 1; }

    IAudioClient* pAudioClient = NULL;
    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
    if (FAILED(hr)) { printf("Activate failed: 0x%08X\n", hr); return 1; }

    WAVEFORMATEX* pwfx = NULL;
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) { printf("GetMixFormat failed: 0x%08X\n", hr); return 1; }

    printf("MixFormat: %u channels, %u Hz, %u bits/sample\n",
           pwfx->nChannels, pwfx->nSamplesPerSec, pwfx->wBitsPerSample);

    REFERENCE_TIME hnsRequestedDuration = 10000000; // 1 second
    hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, hnsRequestedDuration, 0, pwfx, NULL);
    if (FAILED(hr)) { printf("Initialize failed: 0x%08X\n", hr); return 1; }

    IAudioRenderClient* pRenderClient = NULL;
    hr = pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&pRenderClient);
    if (FAILED(hr)) { printf("GetService failed: 0x%08X\n", hr); return 1; }

    UINT32 bufferFrameCount = 0;
    pAudioClient->GetBufferSize(&bufferFrameCount);

    // Grab buffer and fill with 440Hz sine wave for 1.5 seconds
    pAudioClient->Start();
    printf("Playing 1.5 seconds of 440 Hz audio through WASAPI on default device...\n");

    DWORD startTime = GetTickCount();
    float phase = 0.0f;
    float phaseInc = 2.0f * 3.14159265f * 440.0f / (float)pwfx->nSamplesPerSec;

    while (GetTickCount() - startTime < 1500) {
        UINT32 numPadding = 0;
        pAudioClient->GetCurrentPadding(&numPadding);
        UINT32 numFramesAvailable = bufferFrameCount - numPadding;

        if (numFramesAvailable > 0) {
            BYTE* pData = NULL;
            hr = pRenderClient->GetBuffer(numFramesAvailable, &pData);
            if (SUCCEEDED(hr)) {
                if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                    (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                     reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pwfx)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
                    float* pFloats = reinterpret_cast<float*>(pData);
                    for (UINT32 f = 0; f < numFramesAvailable; ++f) {
                        float sample = sinf(phase) * 0.2f;
                        phase += phaseInc;
                        for (WORD c = 0; c < pwfx->nChannels; ++c) {
                            *pFloats++ = sample;
                        }
                    }
                } else if (pwfx->wBitsPerSample == 16) {
                    short* pShorts = reinterpret_cast<short*>(pData);
                    for (UINT32 f = 0; f < numFramesAvailable; ++f) {
                        short sample = (short)(sinf(phase) * 6000.0f);
                        phase += phaseInc;
                        for (WORD c = 0; c < pwfx->nChannels; ++c) {
                            *pShorts++ = sample;
                        }
                    }
                }
                pRenderClient->ReleaseBuffer(numFramesAvailable, 0);
            }
        }
        Sleep(20);
    }

    pAudioClient->Stop();
    printf("WASAPI playback complete!\n");

    CoTaskMemFree(pwfx);
    pRenderClient->Release();
    pAudioClient->Release();
    pDevice->Release();
    pEnumerator->Release();
    CoUninitialize();
    return 0;
}
