#pragma once
#include <miniaudio.h>
#include <vector>
#include <complex>
#include <mutex>
#include <atomic>
#include "Common.h"

struct AtomicAudioData {
    std::atomic<float> bass{0.0f};
    std::atomic<float> mids{0.0f};
    std::atomic<float> treble{0.0f};
    std::atomic<float> bassOnset{0.0f};
    std::atomic<float> midsOnset{0.0f};
    std::atomic<float> trebleOnset{0.0f};
    std::atomic<float> harshness{0.0f};
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    // Initializes the audio device capture stream
    bool init();

    // Captures the latest window of samples, applies Hanning window, runs FFT, and updates atomics
    void update(float dt);

    // Read access to the thread-safe atomic audio data
    const AtomicAudioData& getAudioData() const { return m_atomicAudio; }

private:
    ma_device m_device;
    bool m_initialized;

    // Circular buffer to hold incoming PCM sample frames
    std::vector<float> m_ringBuffer;
    size_t m_writeIndex;
    std::mutex m_bufferMutex;

    // Output frequency data exposed to the Render Thread
    AtomicAudioData m_atomicAudio;

    // Internal smoothed values for organic visuals
    float m_smoothedBass;
    float m_smoothedMids;
    float m_smoothedTreble;
    float m_smoothedHarshness;
    float m_prevRawBass;
    float m_prevRawMids;
    float m_prevRawTreble;

    // Master volume tracking for automatic gain control
    float m_masterVolume;

    // Sliding window FFT storage
    std::vector<std::complex<float>> m_fftBuffer;
    std::vector<float> m_window; // Hanning window cache

    // Thread-safe ring buffer push/read methods
    void pushSamples(const float* samples, unsigned int frameCount);
    void readLatestSamples(std::vector<float>& dest, size_t count);

    // Cooley-Tukey Radix-2 FFT implementation
    void runFFT(std::vector<std::complex<float>>& data);

    // Static callback passed to miniaudio to receive stream updates
    static void audioDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
};
