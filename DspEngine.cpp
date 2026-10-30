#include "DspEngine.h"

DspEngine::DspEngine()
    : m_gain(1.0f),
      m_enabled(true),
      m_superBoost(false)
{
}

DspEngine::~DspEngine() {
}

void DspEngine::SetVolume(int volumePercent) {
    if (volumePercent < 100) volumePercent = 100;
    if (volumePercent > 500) volumePercent = 500;
    // 100% -> 1.0, 500% -> 5.0
    m_gain = (float)volumePercent / 100.0f;
}

void DspEngine::SetEnabled(bool enabled) {
    m_enabled = enabled;
}

void DspEngine::SetSuperBoost(bool enabled) {
    m_superBoost = enabled;
}

void DspEngine::ProcessSamplesFloat(float* buffer, UINT numSamples) {
    if (!m_enabled || (m_gain <= 1.0f && !m_superBoost)) {
        return; // Passthrough
    }

    float g = m_gain;
    for (UINT i = 0; i < numSamples; ++i) {
        float val = buffer[i] * g;
        buffer[i] = SoftLimit(val);
    }
}

void DspEngine::ProcessSamplesInt16(short* buffer, UINT numSamples) {
    if (!m_enabled || (m_gain <= 1.0f && !m_superBoost)) {
        return;
    }

    float g = m_gain;
    const float norm = 1.0f / 32768.0f;
    for (UINT i = 0; i < numSamples; ++i) {
        float sample = (float)buffer[i] * norm * g;
        float limited = SoftLimit(sample);
        int outVal = (int)(limited * 32767.0f);
        if (outVal > 32767) outVal = 32767;
        if (outVal < -32768) outVal = -32768;
        buffer[i] = (short)outVal;
    }
}
