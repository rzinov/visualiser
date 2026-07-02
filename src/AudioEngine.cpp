#define MINIAUDIO_IMPLEMENTATION
#include "AudioEngine.h"
#include <iostream>
#include <algorithm>
#include <cmath>

AudioEngine::AudioEngine() 
    : m_initialized(false)
    , m_writeIndex(0)
    , m_smoothedBass(0.0f)
    , m_smoothedMids(0.0f)
    , m_smoothedTreble(0.0f)
    , m_prevRawBass(0.0f)
    , m_prevRawMids(0.0f)
    , m_prevRawTreble(0.0f)
    , m_smoothedHarshness(0.0f)
    , m_masterVolume(0.1f)
{
    // Configure buffer size (4096 samples is ~93ms of audio history)
    m_ringBuffer.resize(4096, 0.0f);
    m_fftBuffer.resize(512, std::complex<float>(0.0f, 0.0f));

    // Cache Hanning window coefficients to reduce spectral leakage
    m_window.resize(512);
    for (size_t i = 0; i < 512; ++i) {
        m_window[i] = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * i / 511.0f));
    }
}

AudioEngine::~AudioEngine() {
    if (m_initialized) {
        ma_device_uninit(&m_device);
    }
}

// Static callback invoked by the system's underlying audio thread
void AudioEngine::audioDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    AudioEngine* pEngine = static_cast<AudioEngine*>(pDevice->pUserData);
    if (pEngine && pInput) {
        pEngine->pushSamples(static_cast<const float*>(pInput), frameCount);
    }
    (void)pOutput; // Unused in capture-only device
}

bool AudioEngine::init() {
    std::cout << "Initializing Audio Engine via miniaudio..." << std::endl;

    ma_device_config deviceConfig;
    deviceConfig = ma_device_config_init(ma_device_type_loopback);
    deviceConfig.capture.format   = ma_format_f32; // Floating point audio
    deviceConfig.capture.channels = 1;             // Mono
    deviceConfig.sampleRate       = 44100;         // Standard sample rate
    deviceConfig.dataCallback     = audioDataCallback;
    deviceConfig.pUserData        = this;

    if (ma_device_init(NULL, &deviceConfig, &m_device) != MA_SUCCESS) {
        std::cerr << "Failed to initialize miniaudio capture device." << std::endl;
        return false;
    }

    if (ma_device_start(&m_device) != MA_SUCCESS) {
        std::cerr << "Failed to start miniaudio device stream." << std::endl;
        ma_device_uninit(&m_device);
        return false;
    }

    m_initialized = true;
    std::cout << "Audio capture stream started successfully on default device." << std::endl;
    return true;
}

void AudioEngine::pushSamples(const float* samples, unsigned int frameCount) {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    for (unsigned int i = 0; i < frameCount; ++i) {
        m_ringBuffer[m_writeIndex] = samples[i];
        m_writeIndex = (m_writeIndex + 1) % m_ringBuffer.size();
    }
}

void AudioEngine::readLatestSamples(std::vector<float>& dest, size_t count) {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    dest.resize(count);

    // Read backwards from the current write pointer
    size_t bufferSize = m_ringBuffer.size();
    for (size_t i = 0; i < count; ++i) {
        // Find position index moving backwards
        long long index = static_cast<long long>(m_writeIndex) - 1 - i;
        while (index < 0) {
            index += bufferSize;
        }
        // Save backwards but in correct temporal order (dest[count-1-i])
        dest[count - 1 - i] = m_ringBuffer[index % bufferSize];
    }
}

// Bit reversal permutation helper for Cooley-Tukey
static unsigned int reverseBits(unsigned int x, unsigned int n) {
    unsigned int res = 0;
    for (unsigned int i = 0; i < n; i++) {
        if (x & (1 << i)) {
            res |= (1 << (n - 1 - i));
        }
    }
    return res;
}

// In-place iterative Cooley-Tukey Radix-2 FFT
void AudioEngine::runFFT(std::vector<std::complex<float>>& a) {
    unsigned int n = static_cast<unsigned int>(a.size());
    unsigned int log_n = 0;
    while ((1U << log_n) < n) {
        log_n++;
    }

    // Reorder array based on bit reversal
    for (unsigned int i = 0; i < n; i++) {
        unsigned int rev = reverseBits(i, log_n);
        if (i < rev) {
            std::swap(a[i], a[rev]);
        }
    }

    // Butterfly computations
    for (unsigned int len = 2; len <= n; len <<= 1) {
        float angle = -2.0f * 3.14159265f / len;
        std::complex<float> wlen(std::cos(angle), std::sin(angle));
        for (unsigned int i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (unsigned int j = 0; j < len / 2; j++) {
                std::complex<float> u = a[i + j];
                std::complex<float> v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

void AudioEngine::update(float dt) {
    if (!m_initialized) return;

    // Grab the latest 512 samples (sliding window)
    std::vector<float> samples;
    readLatestSamples(samples, 512);

    // Apply Hanning window to prevent spectral leakage
    for (size_t i = 0; i < 512; ++i) {
        m_fftBuffer[i] = std::complex<float>(samples[i] * m_window[i], 0.0f);
    }

    // Process FFT on the Render Thread
    runFFT(m_fftBuffer);

    // Bins map: 44100Hz / 512 samples = ~86Hz per bin.
    // Bass: bins 1 - 3 (86Hz to 258Hz)
    // Mids: bins 4 - 46 (344Hz to ~3950Hz)
    // Treble: bins 47 - 255 (4040Hz to ~22000Hz)

    float bassSum = 0.0f;
    for (int i = 1; i <= 3; ++i) {
        bassSum += std::abs(m_fftBuffer[i]);
    }
    float rawBass = (bassSum / 3.0f) * 15.0f; // Scale factor for visual sensitivity

    float midsSum = 0.0f;
    for (int i = 4; i <= 46; ++i) {
        midsSum += std::abs(m_fftBuffer[i]);
    }
    float rawMids = (midsSum / 43.0f) * 20.0f;

    float trebleSum = 0.0f;
    for (int i = 47; i < 256; ++i) {
        trebleSum += std::abs(m_fftBuffer[i]);
    }
    float rawTreble = (trebleSum / 209.0f) * 35.0f;

    // Clamp values to safe normalized float ranges [0.0, 1.0]
    rawBass   = std::max(0.0f, std::min(rawBass, 1.0f));
    rawMids   = std::max(0.0f, std::min(rawMids, 1.0f));
    rawTreble = std::max(0.0f, std::min(rawTreble, 1.0f));

    // Dynamic smoothing: rapid rise to catch beats, slow decay to maintain glow
    float riseSpeed = 16.0f; // Catch transients instantly
    float fallSpeed = 3.5f;  // Smooth decay for trails

    auto smoothFilter = [](float& current, float target, float dt, float rise, float fall) {
        float speed = (target > current) ? rise : fall;
        current = current + (target - current) * std::min(dt * speed, 1.0f);
    };

    // Check if we are receiving any real audio input (using a lower threshold for high sensitivity)
    bool hasAudio = (rawBass > 0.001f || rawMids > 0.001f || rawTreble > 0.001f);
    
    // Track silence duration
    static float silentTime = 0.0f;
    if (!hasAudio) {
        silentTime += dt;
    } else {
        silentTime = 0.0f;
    }

    float finalBassOnset = 0.0f;
    float finalMidsOnset = 0.0f;
    float finalTrebleOnset = 0.0f;
    float finalHarshness = 0.0f;

    // If silence persists for > 0.5s, fallback to a synthetic dance beat generator
    if (silentTime > 0.5f) {
        // Reset master volume so dynamic range calibrates fresh when music plays
        m_masterVolume = 0.1f;

        static float synthTime = 0.0f;
        synthTime += dt;
        
        // Quiet fallback when paused: tiny, extremely slow drift so it goes into a deep sleep state
        float synthBass   = 0.015f + std::sin(synthTime * 0.2f) * 0.005f;
        float synthMids   = 0.012f + std::cos(synthTime * 0.15f) * 0.004f;
        float synthTreble = 0.008f + std::sin(synthTime * 0.1f) * 0.002f;

        smoothFilter(m_smoothedBass, synthBass, dt, riseSpeed, fallSpeed);
        smoothFilter(m_smoothedMids, synthMids, dt, riseSpeed, fallSpeed);
        smoothFilter(m_smoothedTreble, synthTreble, dt, riseSpeed, fallSpeed);
        smoothFilter(m_smoothedHarshness, 0.0f, dt, riseSpeed, fallSpeed);
        
        finalBassOnset = 0.0f;
        finalMidsOnset = 0.0f;
        finalTrebleOnset = 0.0f;
        finalHarshness = 0.0f;
    } else {
        // 1. Find the maximum raw value in this frame
        float frameMax = std::max(rawBass, std::max(rawMids, rawTreble));

        // 2. Smoothly track the master volume envelope
        // Using a moderate decay rate so that it adapts to overall volume changes over a ~9-second window,
        // but reacts slightly faster to attacks to prevent clipping.
        float volDecayRate = 0.08f;
        float volAttackRate = 0.6f;
        if (frameMax > m_masterVolume) {
            m_masterVolume = m_masterVolume + (frameMax - m_masterVolume) * dt * volAttackRate;
        } else {
            m_masterVolume = m_masterVolume + (frameMax - m_masterVolume) * dt * volDecayRate;
        }
        
        // Floor the master volume to avoid division by zero or amplifying the silent noise floor
        m_masterVolume = std::max(m_masterVolume, 0.04f);

        // 3. Normalize all bands relative to the master volume
        // We multiply m_masterVolume by a headroom scale (e.g. 1.25) so that peaks don't saturate instantly,
        // preserving the difference between loud beats and average elements.
        float normScale = m_masterVolume * 1.25f;
        float normalizedBass   = rawBass / normScale;
        float normalizedMids   = rawMids / normScale;
        float normalizedTreble = rawTreble / normScale;

        // Clamp normalized values to [0.0, 1.0]
        normalizedBass   = std::max(0.0f, std::min(normalizedBass, 1.0f));
        normalizedMids   = std::max(0.0f, std::min(normalizedMids, 1.0f));
        normalizedTreble = std::max(0.0f, std::min(normalizedTreble, 1.0f));

        // Normal real-time loopback capture processing using normalized values
        smoothFilter(m_smoothedBass, normalizedBass, dt, riseSpeed, fallSpeed);
        smoothFilter(m_smoothedMids, normalizedMids, dt, riseSpeed, fallSpeed);
        smoothFilter(m_smoothedTreble, normalizedTreble, dt, riseSpeed, fallSpeed);
        
        // Spectral Flux using normalized bass
        float prevNormalizedBass = m_prevRawBass / normScale;
        float bassOnset = std::max(0.0f, normalizedBass - prevNormalizedBass);
        finalBassOnset = bassOnset;

        // Spectral Flux using normalized mids
        float prevNormalizedMids = m_prevRawMids / normScale;
        float midsOnset = std::max(0.0f, normalizedMids - prevNormalizedMids);
        finalMidsOnset = midsOnset;

        // Spectral Flux using normalized treble
        float prevNormalizedTreble = m_prevRawTreble / normScale;
        float trebleOnset = std::max(0.0f, normalizedTreble - prevNormalizedTreble);
        finalTrebleOnset = trebleOnset;

        // Calculate Spectral Centroid as center of gravity of the spectrum
        float centroidSum = 0.0f;
        float amplitudeSum = 0.0f;
        for (int i = 1; i < 256; ++i) {
            float amp = std::abs(m_fftBuffer[i]);
            centroidSum += amp * float(i);
            amplitudeSum += amp;
        }
        float spectralCentroid = (amplitudeSum > 0.001f) ? (centroidSum / amplitudeSum) : 0.0f;

        // Calculate High-Frequency Energy Ratio (high-mids and treble relative to total)
        float highEnergy = 0.0f;
        for (int i = 30; i < 256; ++i) {
            highEnergy += std::abs(m_fftBuffer[i]);
        }
        float highRatio = (amplitudeSum > 0.001f) ? (highEnergy / amplitudeSum) : 0.0f;

        // Normalize and combine centroid and high ratio
        float normalizedCentroid = std::min(1.0f, std::max(0.0f, (spectralCentroid - 25.0f) / 95.0f));
        float normalizedRatio = std::min(1.0f, std::max(0.0f, highRatio / 0.55f));
        float rawHarshness = std::max(normalizedCentroid * 0.7f + normalizedRatio * 0.3f, normalizedRatio * 0.8f);
        rawHarshness = std::min(rawHarshness, 1.0f);

        // Smoothly filter harshness
        smoothFilter(m_smoothedHarshness, rawHarshness, dt, 8.0f, 3.0f);
        finalHarshness = m_smoothedHarshness;
    }

    m_prevRawBass = rawBass;
    m_prevRawMids = rawMids;
    m_prevRawTreble = rawTreble;

    // Store thread-safe atomic outputs
    m_atomicAudio.bass.store(m_smoothedBass);
    m_atomicAudio.mids.store(m_smoothedMids);
    m_atomicAudio.treble.store(m_smoothedTreble);
    m_atomicAudio.bassOnset.store(finalBassOnset);
    m_atomicAudio.midsOnset.store(finalMidsOnset);
    m_atomicAudio.trebleOnset.store(finalTrebleOnset);
    m_atomicAudio.harshness.store(finalHarshness);
}
