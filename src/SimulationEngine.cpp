#include "SimulationEngine.h"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>

SimulationEngine::SimulationEngine() : m_ssbo(0), m_particleCount(PARTICLE_COUNT) {}

SimulationEngine::~SimulationEngine() {
    if (m_ssbo != 0) {
        glDeleteBuffers(1, &m_ssbo);
    }
}

bool SimulationEngine::init() {
    // 1. Compile compute shader
    std::string computePath = std::string(SHADER_DIR) + "fluid.comp";
    std::cout << "Compiling compute shader: " << computePath << std::endl;
    if (!m_computeShader.loadCompute(computePath)) {
        std::cerr << "Failed to compile compute shader!" << std::endl;
        return false;
    }

    // 2. Initialize and upload particles to SSBO
    initParticles();
    return true;
}

void SimulationEngine::reallocateBuffer(int newCount) {
    if (newCount < 1024) newCount = 1024; // Lower boundary safety
    
    std::cout << "[RenderThread] Reallocating SSBO from " << m_particleCount 
              << " to " << newCount << " particles..." << std::endl;

    if (m_ssbo != 0) {
        glDeleteBuffers(1, &m_ssbo);
        m_ssbo = 0;
    }

    m_particleCount = newCount;
    initParticles();
}

void SimulationEngine::initParticles() {
    // Seed random number generator
    srand(static_cast<unsigned int>(time(NULL)));

    std::vector<Particle> particles(m_particleCount);

    for (int i = 0; i < m_particleCount; ++i) {
        // Random position in NDC space (-1.0 to 1.0)
        float px = (static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f;
        float py = (static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f;
        
        // Species division: 100 Bass Leviathans (0.0f), 5000 Mid Ribbons (1.0f), rest Treble Swarm (2.0f)
        float pz = 2.0f;
        if (i < 100) {
            pz = 0.0f;
        } else if (i < 5100) {
            pz = 1.0f;
        }
        
        float life = (static_cast<float>(rand()) / RAND_MAX);

        particles[i].position_life = glm::vec4(px, py, pz, life);

        // Initial velocity direction (0 to 2*PI) and speed based on species
        float angle = (static_cast<float>(rand()) / RAND_MAX) * 2.0f * 3.14159265f;
        float speed = 0.0f;
        if (pz == 0.0f) {
            // Bass Leviathans move slow
            speed = 0.005f + (static_cast<float>(rand()) / RAND_MAX) * 0.01f;
        } else if (pz == 1.0f) {
            speed = 0.01f + (static_cast<float>(rand()) / RAND_MAX) * 0.03f;
        } else {
            speed = 0.02f + (static_cast<float>(rand()) / RAND_MAX) * 0.08f;
        }
        
        float vx = std::cos(angle) * speed;
        float vy = std::sin(angle) * speed;
        float vz = 0.0f;
        float mass = 1.0f;

        particles[i].velocity_mass = glm::vec4(vx, vy, vz, mass);
    }

    // Allocate SSBO on GPU
    glGenBuffers(1, &m_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, m_particleCount * sizeof(Particle), particles.data(), GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    std::cout << "[RenderThread] Reallocated VRAM: " << m_particleCount << " particles (" 
              << (m_particleCount * sizeof(Particle)) / (1024.0 * 1024.0) << " MB allocated)." << std::endl;
}

extern float g_coeffShift;

void SimulationEngine::update(float dt, float time, float audioTime, const AudioData& audio, const SimulationConfig& config, glm::vec2 mousePos, int mouseInteraction, float beatRepelStrength, float lightningActive, glm::vec2 lightningStart, glm::vec2 lightningEnd, float leftGravity, float rightGravity, glm::vec2 windForce, float hype, float harshness, float aspect) {
    m_computeShader.use();

    // Bind SSBO to binding index 0
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_ssbo);

    // Apply dtScale from settings to control simulation pace
    float scaledDt = dt * config.dtScale;

    // Shift attractor coefficients dynamically on bass beats and real-time audio bands (keeps shapes evolving)
    float currentA = config.attractorA + std::sin(g_coeffShift * 1.5f) * 0.45f + (audio.bass - audio.treble) * 0.40f;
    float currentB = config.attractorB + std::cos(g_coeffShift * 1.2f) * 0.40f + audio.mids * 0.30f;
    float currentC = config.attractorC + std::sin(g_coeffShift * 0.9f) * 0.45f - audio.treble * 0.35f;
    float currentD = config.attractorD + std::cos(g_coeffShift * 1.6f) * 0.40f + audio.bass * 0.40f;

    // Set core uniforms
    m_computeShader.setFloat("u_time", time);
    m_computeShader.setFloat("u_audioTime", audioTime);
    m_computeShader.setFloat("u_delta_time", scaledDt);
    m_computeShader.setUInt("u_num_particles", m_particleCount);
    m_computeShader.setFloat("u_aspect", aspect);

    // Set attractor coefficients
    m_computeShader.setFloat("u_a", currentA);
    m_computeShader.setFloat("u_b", currentB);
    m_computeShader.setFloat("u_c", currentC);
    m_computeShader.setFloat("u_d", currentD);

    // Set audio uniforms (scale inputs by global master sensitivity)
    m_computeShader.setFloat("u_bass", audio.bass * config.masterSensitivity);
    m_computeShader.setFloat("u_mids", audio.mids * config.masterSensitivity);
    m_computeShader.setFloat("u_treble", audio.treble * config.masterSensitivity);
    m_computeShader.setFloat("u_bass_onset", audio.bassOnset * config.masterSensitivity);
    m_computeShader.setFloat("u_kickImpact", config.kickImpact);
    
    // Compute dynamic genre flow topology ratio (bass vs treble intensity)
    float bassTrebleRatio = audio.bass / (audio.treble + 0.01f);
    m_computeShader.setFloat("u_bassTrebleRatio", bassTrebleRatio);

    // Set Curl Noise Parameters from ImGui Control Panel
    m_computeShader.setFloat("u_curlEpsilon", config.curlEpsilon);
    m_computeShader.setFloat("u_noiseFreq", config.noiseFreq);
    m_computeShader.setFloat("u_noiseEvolSpeed", config.noiseEvolSpeed);
    m_computeShader.setFloat("u_fluidDrag", config.fluidDrag);
    m_computeShader.setFloat("u_smokeDiffusion", config.smokeDiffusion);

    // Set Mouse Interaction Uniforms
    m_computeShader.setVec2("u_mousePos", mousePos);
    m_computeShader.setInt("u_mouseInteraction", mouseInteraction);
    m_computeShader.setFloat("u_mouseGravity", config.mouseGravity);
    m_computeShader.setFloat("u_beatRepelStrength", beatRepelStrength);
    m_computeShader.setFloat("u_lightningActive", lightningActive);
    m_computeShader.setVec2("u_lightningStart", lightningStart);
    m_computeShader.setVec2("u_lightningEnd", lightningEnd);

    // Set Call & Response / Hype Uniforms
    m_computeShader.setFloat("u_leftGravity", leftGravity);
    m_computeShader.setFloat("u_rightGravity", rightGravity);
    m_computeShader.setVec2("u_windForce", windForce);
    m_computeShader.setFloat("u_hype", hype);
    m_computeShader.setFloat("u_harshness", harshness);
    m_computeShader.setInt("u_shoegazeMode", config.shoegazeMode ? 1 : 0);

    // Calculate execution workgroups
    unsigned int numGroups = (m_particleCount + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;

    // Dispatch the compute threads
    glDispatchCompute(numGroups, 1, 1);

    // Block until compute shader completes
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Unbind
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
}
