#pragma once
#include <glm/glm.hpp>

// Particle struct layout matching std430 layout rules in GLSL
// We use position_life.z to store the Species ID (0.0: Bass, 1.0: Mids, 2.0: Treble Swarm)
struct Particle {
    glm::vec4 position_life; // xy = position, z = Species ID, w = life (0.0 to 1.0)
    glm::vec4 velocity_mass; // xyz = velocity, w = mass
};

// Local thread snapshot of audio data
struct AudioData {
    float bass = 0.0f;
    float mids = 0.0f;
    float treble = 0.0f;
    float bassOnset = 0.0f;
    float midsOnset = 0.0f;
    float trebleOnset = 0.0f;
    float harshness = 0.0f;
};

// Settings control panel structure
struct SimulationConfig {
    // Simulation Parameters
    int particleCount = 1000000;
    float dtScale = 1.0f;

    // Curl Noise Parameters
    float curlEpsilon = 0.008f;
    float noiseFreq = 1.6f;
    float noiseEvolSpeed = 0.2f;
    float fluidDrag = 0.45f;
    float smokeDiffusion = 0.25f; // Diffusion random walk coefficient

    // Audio Sensitivity
    float masterSensitivity = 1.5f;
    float kickImpact = 25.0f;
    float mouseGravity = 120.0f;

    // Call & Response / Hype
    bool callResponseMode = true;
    float callResponseGravity = 150.0f;
    float callResponseWind = 8.0f;

    // Attractor Parameters
    float attractorA = -1.4f;
    float attractorB = 1.6f;
    float attractorC = 1.0f;
    float attractorD = 0.7f;
    int symmetryN = 8;

    // Rendering Parameters
    float baseScale = 0.005f;
    float flareSensitivity = 3.0f;
    float trailDecay = 0.80f;
    float bloomIntensity = 0.8f;
    float exposure = 1.0f;
    float particleOpacity = 0.40f; // swarm brightness multiplier
    float colorSaturation = 1.5f;  // color saturation boost
    bool shoegazeMode = false;
};

// Simulation settings
const unsigned int WINDOW_WIDTH = 1920;
const unsigned int WINDOW_HEIGHT = 1080;
const unsigned int PARTICLE_COUNT = 1000000;
const unsigned int WORK_GROUP_SIZE = 1024;
