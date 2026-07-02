#pragma once
#include <glad/glad.h>
#include "Shader.h"
#include "Common.h"

class SimulationEngine {
public:
    SimulationEngine();
    ~SimulationEngine();

    // Initializes shaders and allocates/initializes SSBO in VRAM
    bool init();

    // Dispatches the GPU compute shader to update positions (accepts audio and dynamic configs)
    void update(float dt, float time, float audioTime, const AudioData& audio, const SimulationConfig& config, glm::vec2 mousePos, int mouseInteraction, float beatRepelStrength = 0.0f, float lightningActive = 0.0f, glm::vec2 lightningStart = glm::vec2(0.0f), glm::vec2 lightningEnd = glm::vec2(0.0f), float leftGravity = 0.0f, float rightGravity = 0.0f, glm::vec2 windForce = glm::vec2(0.0f), float hype = 0.0f, float harshness = 0.0f, float aspect = 1.777f);

    // Safely reallocates the VRAM particle SSBO on the Render Thread
    void reallocateBuffer(int newCount);

    // Getters for integration with the render pipeline
    unsigned int getSSBO() const { return m_ssbo; }
    int getParticleCount() const { return m_particleCount; }

private:
    unsigned int m_ssbo;
    Shader m_computeShader;
    int m_particleCount; // Tracks currently allocated particle count in VRAM

    // Helper to generate the initial particle pool on the CPU
    void initParticles();
};
