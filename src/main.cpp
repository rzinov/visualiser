#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>
#include <string>

#include "Common.h"
#include "imgui.h"
#include "SimulationEngine.h"
#include "RenderPipeline.h"
#include "AudioEngine.h"

// Global pointers to visualizer modules for callback access
SimulationEngine* g_simEngine = nullptr;
RenderPipeline* g_renderPipeline = nullptr;
AudioEngine* g_audioEngine = nullptr;
SimulationConfig g_config;

// Global timing and performance counters
double g_lastFrameTime = 0.0;
int g_frameCount = 0;
double g_fpsLastTime = 0.0;
int g_fps = 0;
float g_audioTime = 0.0f;
float g_coeffShift = 0.0f;
float g_beatRepel = 0.0f;

// Lightning strike state
float g_lightningActive = 0.0f;
glm::vec2 g_lightningStart = glm::vec2(0.0f);
glm::vec2 g_lightningEnd = glm::vec2(0.0f);
float g_lightningCooldown = 0.0f;

// Call & Response / Hype states
float g_leftPulse = 0.0f;
float g_rightPulse = 0.0f;
float g_hypeFactor = 0.0f;

// Dynamic rotation states
float g_rotationAngle = 0.0f;
float g_rotationSpeed = 0.05f;

// The unified single-threaded frame function
void runFrame(GLFWwindow* window) {
    if (!g_simEngine || !g_renderPipeline || !g_audioEngine) {
        return;
    }

    // 1. Check and handle window resize
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    g_renderPipeline->checkResize(width, height);

    // 2. Precision timing
    double currentTime = glfwGetTime();
    double rawDeltaTime = currentTime - g_lastFrameTime;
    g_lastFrameTime = currentTime;

    // Clamp dt to prevent physics explosions during hiccups
    float dt = std::min(static_cast<float>(rawDeltaTime), 0.1f);
    float time = static_cast<float>(currentTime);

    // 3. Update audio data via loopback capture
    g_audioEngine->update(dt);
    AudioData audio;
    audio.bass   = g_audioEngine->getAudioData().bass.load();
    audio.mids   = g_audioEngine->getAudioData().mids.load();
    audio.treble = g_audioEngine->getAudioData().treble.load();
    audio.bassOnset = g_audioEngine->getAudioData().bassOnset.load();
    audio.midsOnset = g_audioEngine->getAudioData().midsOnset.load();
    audio.trebleOnset = g_audioEngine->getAudioData().trebleOnset.load();
    audio.harshness = g_audioEngine->getAudioData().harshness.load();

    // Accumulate dynamic audio time (dance clock) driven by the song's energy
    // Fallback to a baseline accumulation rate in silence so colors cycle and noise flows
    float audioRate = audio.bass + audio.mids;
    if (audioRate < 0.05f) {
        audioRate = 0.15f;
    }
    g_audioTime += audioRate * dt;

    // Accumulate coefficient shift on bass kick transients to mutate attractor shapes
    if (audio.bassOnset > 0.15f) {
        g_coeffShift += audio.bassOnset * 0.18f;
    }

    // 4. Safe SSBO reallocation on particle count slider changes
    if (g_config.particleCount != g_simEngine->getParticleCount()) {
        g_simEngine->reallocateBuffer(g_config.particleCount);
    }

    // 5. Update particle physics compute shader (passing mouse inputs and accumulated audio time)
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    int leftState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    int rightState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);

    // Convert from screen space (top-left 0,0) to attractor coordinate space (center 0,0)
    float ndcX = (static_cast<float>(xpos) / static_cast<float>(width)) * 2.0f - 1.0f;
    float ndcY = 1.0f - (static_cast<float>(ypos) / static_cast<float>(height)) * 2.0f;
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    glm::vec2 mousePosAttr = glm::vec2((ndcX * aspect) / 0.45f, ndcY / 0.45f);

    // Avoid triggering gravity forces when interacting with the ImGui control panel
    int mouseInteraction = 0; // 0 = none, 1 = repel, 2 = attract
    if (!ImGui::GetIO().WantCaptureMouse) {
        if (leftState == GLFW_PRESS) {
            mouseInteraction = 1;
        } else if (rightState == GLFW_PRESS) {
            mouseInteraction = 2;
        }
    }

    // Detect sudden bass beat drops (isolated transient spikes)
    // We check if bassOnset is high and if we aren't already in the middle of a shockwave
    if (audio.bassOnset > 0.40f && g_beatRepel < 50.0f) {
        g_beatRepel = 450.0f; // Massive central push strength
    }
    // Decay the repel force over time (~0.37s)
    g_beatRepel = std::max(0.0f, g_beatRepel - dt * 1200.0f);

    // Decay the lightning active strength and its trigger cooldown
    g_lightningActive = std::max(0.0f, g_lightningActive - dt * 4.5f); // ~0.22s decay
    g_lightningCooldown = std::max(0.0f, g_lightningCooldown - dt);

    // Trigger a new lightning strike on extreme bass or treble onset spikes
    if ((audio.bassOnset > 0.55f || audio.trebleOnset > 0.55f) && g_lightningCooldown <= 0.0f) {
        g_lightningActive = 1.0f;
        g_lightningCooldown = 1.8f; // Cooldown of 1.8 seconds between strikes
        
        // Choose random start and end points crossing the screen in NDC space [-1.1, 1.1]
        float side = (rand() % 2 == 0) ? -1.1f : 1.1f;
        float startY = ((static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f) * 0.9f;
        float endY = ((static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f) * 0.9f;
        
        g_lightningStart = glm::vec2(side, startY);
        g_lightningEnd = glm::vec2(-side, endY);
    }

    // Dynamic Rotation angle calculation:
    // Bass and mids speed up the rotation, and treble transients reverse direction!
    float targetRotSpeed = 0.05f + (audio.bass * 0.15f) + (audio.mids * 0.08f);
    if (audio.trebleOnset > 0.45f) {
        g_rotationSpeed = -g_rotationSpeed; // Reverse direction
    }
    float rotSign = (g_rotationSpeed >= 0.0f) ? 1.0f : -1.0f;
    g_rotationSpeed = rotSign * (std::abs(g_rotationSpeed) + (targetRotSpeed - std::abs(g_rotationSpeed)) * dt * 4.0f);
    g_rotationAngle += g_rotationSpeed * dt;

    // Pulse states decay (slower decay during intense drop parts to "hold down gravity")
    float pulseDecayRate = (g_hypeFactor > 0.35f) ? 1.2f : 5.0f;
    g_leftPulse = std::max(0.0f, g_leftPulse - dt * pulseDecayRate);
    g_rightPulse = std::max(0.0f, g_rightPulse - dt * pulseDecayRate);

    // Compute Hype Factor based on drum pattern density (moderate attack and slow decay)
    float drumSum = audio.bassOnset * 1.0f + audio.midsOnset * 0.7f + audio.trebleOnset * 0.3f;
    float targetHype = std::min(drumSum * 0.8f, 0.6f);
    g_hypeFactor += (targetHype - g_hypeFactor) * dt * (targetHype > g_hypeFactor ? 2.0f : 0.5f);

    // Drum transient triggers for Left/Right Call & Response (direct max mapping)
    if (audio.bassOnset > 0.18f) {
        g_leftPulse = std::max(g_leftPulse, std::min(1.0f, audio.bassOnset * 1.3f));
    }
    if (audio.midsOnset > 0.18f) {
        g_rightPulse = std::max(g_rightPulse, std::min(1.0f, audio.midsOnset * 1.3f));
    }

    // Calculate active wind and gravity pulses for Call & Response
    float activeLeftGravity = g_config.callResponseMode ? g_leftPulse * g_config.callResponseGravity : 0.0f;
    float activeRightGravity = g_config.callResponseMode ? g_rightPulse * g_config.callResponseGravity : 0.0f;
    glm::vec2 activeWindForce = g_config.callResponseMode ? glm::vec2((g_leftPulse - g_rightPulse) * g_config.callResponseWind, 0.0f) : glm::vec2(0.0f);

    g_simEngine->update(dt, time, g_audioTime, audio, g_config, mousePosAttr, mouseInteraction, g_beatRepel, g_lightningActive, g_lightningStart, g_lightningEnd, activeLeftGravity, activeRightGravity, activeWindForce, g_hypeFactor, audio.harshness, aspect);

    // 6. Execute FBO post-processing passes and render ImGui controls
    g_renderPipeline->render(g_simEngine->getSSBO(), g_simEngine->getParticleCount(), time, g_audioTime, dt, audio, g_config, window, g_lightningActive, g_lightningStart, g_lightningEnd, g_leftPulse, g_rightPulse, g_hypeFactor, audio.harshness, g_rotationAngle);

    // 7. Swap window buffers
    glfwSwapBuffers(window);

    // 8. FPS statistic reporting in window title
    g_frameCount++;
    if (currentTime - g_fpsLastTime >= 1.0) {
        g_fps = g_frameCount;
        g_frameCount = 0;
        g_fpsLastTime = currentTime;

        std::string title = "Celestial Gravity Game | FPS: " + std::to_string(g_fps) + 
                            " | Particles: " + std::to_string(g_simEngine->getParticleCount()) + 
                            " | Chevron Scale: " + std::to_string(g_config.baseScale);
        glfwSetWindowTitle(window, title.c_str());
    }
}

int main() {
    std::cout << "=== PHASE 3: LAUNCHING SINGLE-THREADED CELESTIAL GRAVITY VISUALIZER ===" << std::endl;

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW." << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Celestial Gravity Game | Phase 3", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return -1;
    }

    // Keep OpenGL context current on the Main Thread
    glfwMakeContextCurrent(window);

    // Initialize GLAD loaders
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GPU Renderer: " << glGetString(GL_RENDERER) << std::endl;

    // Get initial framebuffer size for textures
    int initW, initH;
    glfwGetFramebufferSize(window, &initW, &initH);

    // Instantiate engine components
    SimulationEngine simEngine;
    RenderPipeline renderPipeline;
    AudioEngine audioEngine;

    // Bind globals for access in refresh callback
    g_simEngine = &simEngine;
    g_renderPipeline = &renderPipeline;
    g_audioEngine = &audioEngine;

    // Initialize systems
    if (!simEngine.init()) {
        std::cerr << "Failed to initialize Simulation Engine." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    if (!renderPipeline.init(initW, initH, window)) {
        std::cerr << "Failed to initialize Render Pipeline." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    if (!audioEngine.init()) {
        std::cerr << "WARNING: Audio stream failed to start. Running in silent mode." << std::endl;
    }

    // Setup initial timing
    g_lastFrameTime = glfwGetTime();
    g_fpsLastTime = g_lastFrameTime;

    // Register refresh callback to keep rendering smooth during modal drag/resize operations
    glfwSetWindowRefreshCallback(window, [](GLFWwindow* w) {
        runFrame(w);
    });

    std::cout << "All visualizer systems operational. Running main loop." << std::endl;

    // Main event loop (running entirely on the Main Thread)
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents(); 
        runFrame(window);
    }

    // Clean up resources
    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "Visualizer terminated successfully." << std::endl;
    return 0;
}
