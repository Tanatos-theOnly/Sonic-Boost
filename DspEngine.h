#pragma once
#include <windows.h>
#include <cmath>

class DspEngine {
public:
    DspEngine();
    ~DspEngine();

    void SetVolume(int volumePercent); // 100 to 500
    void SetEnabled(bool enabled);
    void SetSuperBoost(bool enabled);

    // Process 32-bit floating point audio in-place
    void ProcessSamplesFloat(float* buffer, UINT numSamples);

    // Process 16-bit signed PCM audio in-place
    void ProcessSamplesInt16(short* buffer, UINT numSamples);

    float GetGain() const { return m_gain; }

private:
    float m_gain;
    bool m_enabled;
    bool m_superBoost;

    // Fast soft saturation limiter function
    inline float SoftLimit(float sample) {
        if (!m_superBoost) {
            // Hard clamp at [-1.0, 1.0] if SuperBoost compressor is disabled
            if (sample > 1.0f) return 1.0f;
            if (sample < -1.0f) return -1.0f;
            return sample;
        }

        // Soft-knee limiter / saturation curve:
        // Preserves linear dynamics up to 0.75 (-2.5 dBFS), then smoothly compresses to 0.99
        const float threshold = 0.75f;
        if (sample > threshold) {
            float excess = sample - threshold;
            return threshold + (1.0f - threshold) * std::tanh(excess / (1.0f - threshold));
        } else if (sample < -threshold) {
            float excess = sample + threshold;
            return -threshold + (1.0f - threshold) * std::tanh(excess / (1.0f - threshold));
        }
        return sample;
    }
};
