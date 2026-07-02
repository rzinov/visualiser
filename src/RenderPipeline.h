#pragma once
#include <glad/glad.h>
#include "Shader.h"
#include "Common.h"
#include <GLFW/glfw3.h>

class RenderPipeline {
public:
    RenderPipeline();
    ~RenderPipeline();

    // Compiles graphics/post-processing shaders, configures GL state, and triggers initial FBO build.
    // Also initializes Dear ImGui context and backends.
    bool init(int initialWidth, int initialHeight, GLFWwindow* window);

    // Checks if the window resolution changed on the main thread and re-allocates FBOs if necessary
    void checkResize(int width, int height);

    // Performs the multi-pass post-processing pipeline and renders to the screen (including Dear ImGui overlay)
    void render(unsigned int particleSsbo, int numParticles, float time, float audioTime, float dt, const AudioData& audio, SimulationConfig& config, GLFWwindow* window, float lightningActive = 0.0f, glm::vec2 lightningStart = glm::vec2(0.0f), glm::vec2 lightningEnd = glm::vec2(0.0f), float leftPulse = 0.0f, float rightPulse = 0.0f, float hype = 0.0f, float harshness = 0.0f, float rotationAngle = 0.0f);

    // Getters / Setters
    void setPointSize(float size) { m_pointSize = size; }
    float getPointSize() const { return m_pointSize; }

private:
    // Shader pipelines
    Shader m_particleShader;
    Shader m_feedbackShader;
    Shader m_bloomThresholdShader;
    Shader m_blurShader;
    Shader m_compositeShader;

    // Vertex attributes
    unsigned int m_particleVao;
    unsigned int m_meshVbo;        // Holds the 3D chevron geometry in VRAM
    unsigned int m_postProcessVao; // Empty VAO for full-screen pass

    // Frame Buffer Objects (FBOs) and corresponding Textures
    int m_width;
    int m_height;

    // Ping-pong buffers for temporal feedback
    unsigned int m_fboSceneA, m_fboSceneB;
    unsigned int m_texSceneA, m_texSceneB;

    // Bloom extraction downsampled buffer (1/4 size)
    unsigned int m_fboBloomThreshold;
    unsigned int m_texBloomThreshold;

    // Bloom Gaussian blur ping-pong buffer (1/4 size)
    unsigned int m_fboBloomBlur;
    unsigned int m_texBloomBlur;

    // Dynamic settings
    float m_pointSize;
    bool m_imguiInitialized;

    // Internal helper methods
    void allocateBuffers();
    void deallocateBuffers();
    void drawFullScreenQuad();
    void buildChevronMesh();
};
