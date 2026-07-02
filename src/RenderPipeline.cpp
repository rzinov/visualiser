#include "RenderPipeline.h"
#include <iostream>
#include <string>
#include <algorithm>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

RenderPipeline::RenderPipeline()
    : m_particleVao(0)
    , m_meshVbo(0)
    , m_postProcessVao(0)
    , m_width(0)
    , m_height(0)
    , m_fboSceneA(0), m_fboSceneB(0)
    , m_texSceneA(0), m_texSceneB(0)
    , m_fboBloomThreshold(0), m_texBloomThreshold(0)
    , m_fboBloomBlur(0), m_texBloomBlur(0)
    , m_pointSize(4.0f)
    , m_imguiInitialized(false)
{}

RenderPipeline::~RenderPipeline() {
    // Shutdown Dear ImGui
    if (m_imguiInitialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    deallocateBuffers();
    if (m_particleVao != 0) glDeleteVertexArrays(1, &m_particleVao);
    if (m_meshVbo != 0) glDeleteBuffers(1, &m_meshVbo);
    if (m_postProcessVao != 0) glDeleteVertexArrays(1, &m_postProcessVao);
}

void RenderPipeline::deallocateBuffers() {
    if (m_fboSceneA != 0) { glDeleteFramebuffers(1, &m_fboSceneA); m_fboSceneA = 0; }
    if (m_fboSceneB != 0) { glDeleteFramebuffers(1, &m_fboSceneB); m_fboSceneB = 0; }
    if (m_fboBloomThreshold != 0) { glDeleteFramebuffers(1, &m_fboBloomThreshold); m_fboBloomThreshold = 0; }
    if (m_fboBloomBlur != 0) { glDeleteFramebuffers(1, &m_fboBloomBlur); m_fboBloomBlur = 0; }

    if (m_texSceneA != 0) { glDeleteTextures(1, &m_texSceneA); m_texSceneA = 0; }
    if (m_texSceneB != 0) { glDeleteTextures(1, &m_texSceneB); m_texSceneB = 0; }
    if (m_texBloomThreshold != 0) { glDeleteTextures(1, &m_texBloomThreshold); m_texBloomThreshold = 0; }
    if (m_texBloomBlur != 0) { glDeleteTextures(1, &m_texBloomBlur); m_texBloomBlur = 0; }
}

void RenderPipeline::allocateBuffers() {
    deallocateBuffers();

    std::cout << "Allocating post-processing HDR framebuffers at resolution " << m_width << "x" << m_height << "..." << std::endl;

    auto createHDRTexture = [](unsigned int& tex, int w, int h) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };

    auto createFBO = [](unsigned int& fbo, unsigned int tex) {
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
        }
    };

    createHDRTexture(m_texSceneA, m_width, m_height);
    createFBO(m_fboSceneA, m_texSceneA);

    createHDRTexture(m_texSceneB, m_width, m_height);
    createFBO(m_fboSceneB, m_texSceneB);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fboSceneA);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fboSceneB);
    glClear(GL_COLOR_BUFFER_BIT);

    int bloomW = m_width / 4;
    int bloomH = m_height / 4;
    createHDRTexture(m_texBloomThreshold, bloomW, bloomH);
    createFBO(m_fboBloomThreshold, m_texBloomThreshold);

    createHDRTexture(m_texBloomBlur, bloomW, bloomH);
    createFBO(m_fboBloomBlur, m_texBloomBlur);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderPipeline::buildChevronMesh() {
    // A streamlined 3D tetrahedron pointing along +X (local forward)
    // Consists of 4 faces (12 vertices drawn as triangles)
    float vertices[] = {
        // Face 1 (Tip - Back Left - Back Right)
        0.025f, 0.0f, 0.0f,
        -0.015f, 0.01f, 0.005f,
        -0.015f, -0.01f, 0.005f,

        // Face 2 (Tip - Back Right - Back Bottom)
        0.025f, 0.0f, 0.0f,
        -0.015f, -0.01f, 0.005f,
        -0.015f, 0.0f, -0.01f,

        // Face 3 (Tip - Back Bottom - Back Left)
        0.025f, 0.0f, 0.0f,
        -0.015f, 0.0f, -0.01f,
        -0.015f, 0.01f, 0.005f,

        // Face 4 (Back Bottom - Back Right - Back Left - Cap Base)
        -0.015f, 0.0f, -0.01f,
        -0.015f, -0.01f, 0.005f,
        -0.015f, 0.01f, 0.005f
    };

    glBindVertexArray(m_particleVao);
    
    glGenBuffers(1, &m_meshVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_meshVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

bool RenderPipeline::init(int initialWidth, int initialHeight, GLFWwindow* window) {
    m_width = initialWidth;
    m_height = initialHeight;

    std::string shaderRoot = SHADER_DIR;
    std::cout << "Compiling instanced particle graphics shader..." << std::endl;
    if (!m_particleShader.loadGraphics(shaderRoot + "particle.vert", shaderRoot + "particle.frag")) return false;

    std::cout << "Compiling feedback warp shader..." << std::endl;
    if (!m_feedbackShader.loadGraphics(shaderRoot + "post_process.vert", shaderRoot + "feedback.frag")) return false;

    std::cout << "Compiling bloom extraction shader..." << std::endl;
    if (!m_bloomThresholdShader.loadGraphics(shaderRoot + "post_process.vert", shaderRoot + "bloom_threshold.frag")) return false;

    std::cout << "Compiling Gaussian blur shader..." << std::endl;
    if (!m_blurShader.loadGraphics(shaderRoot + "post_process.vert", shaderRoot + "blur.frag")) return false;

    std::cout << "Compiling post-processing compositing shader..." << std::endl;
    if (!m_compositeShader.loadGraphics(shaderRoot + "post_process.vert", shaderRoot + "composite.frag")) return false;

    // Generate VAOs
    glGenVertexArrays(1, &m_particleVao);
    glGenVertexArrays(1, &m_postProcessVao);

    // Build the 3D chevron VBO and assign it to VAO
    buildChevronMesh();

    // Initial FBO allocation
    allocateBuffers();

    // Configure Dear ImGui Context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    // Disable saving/loading imgui.ini to avoid clutter
    io.IniFilename = NULL;

    // Enable multi-viewport support so panels can be dragged outside the window
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    // Initialize ImGui backends.
    // Install callbacks automatically since we run single-threaded on the Main Thread.
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");
    m_imguiInitialized = true;

    // Enable GL states
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    return true;
}

void RenderPipeline::checkResize(int width, int height) {
    if (width > 0 && height > 0 && (width != m_width || height != m_height)) {
        m_width = width;
        m_height = height;
        allocateBuffers();
    }
}

void RenderPipeline::drawFullScreenQuad() {
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void RenderPipeline::render(unsigned int particleSsbo, int numParticles, float time, float audioTime, float dt, const AudioData& audio, SimulationConfig& config, GLFWwindow* window, float lightningActive, glm::vec2 lightningStart, glm::vec2 lightningEnd, float leftPulse, float rightPulse, float hype, float harshness, float rotationAngle) {
    // --- STEP 0: IMGUI NEW FRAME ---
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Create Simulation Settings UI
    ImGui::SetNextWindowSize(ImVec2(400, 480), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    
    ImGui::Begin("Simulation Control Panel", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Presets:");
    ImGui::SameLine();
    if (ImGui::Button("Rock", ImVec2(85, 24))) {
        config.dtScale = 1.1f;
        config.curlEpsilon = 0.015f;
        config.noiseFreq = 1.8f;
        config.noiseEvolSpeed = 0.35f;
        config.fluidDrag = 0.40f;
        config.smokeDiffusion = 0.35f;
        config.masterSensitivity = 1.8f;
        config.kickImpact = 45.0f;
        config.trailDecay = 0.82f;      // tuned
        config.particleOpacity = 0.30f;  // tuned
        config.colorSaturation = 1.5f;
        config.callResponseMode = true;
        config.callResponseGravity = 140.0f;
        config.callResponseWind = 9.0f;
        config.shoegazeMode = false;
        config.symmetryN = 8;
    }
    ImGui::SameLine();
    if (ImGui::Button("Rap", ImVec2(85, 24))) {
        config.dtScale = 0.95f;
        config.curlEpsilon = 0.010f;
        config.noiseFreq = 1.2f;
        config.noiseEvolSpeed = 0.15f;
        config.fluidDrag = 0.55f;
        config.smokeDiffusion = 0.15f;
        config.masterSensitivity = 2.0f;
        config.kickImpact = 85.0f;
        config.trailDecay = 0.85f;      // tuned
        config.particleOpacity = 0.35f;  // tuned
        config.colorSaturation = 1.8f;
        config.callResponseMode = true;
        config.callResponseGravity = 200.0f;
        config.callResponseWind = 7.0f;
        config.shoegazeMode = false;
        config.symmetryN = 8;
    }
    ImGui::SameLine();
    if (ImGui::Button("Chill", ImVec2(85, 24))) {
        config.dtScale = 0.60f;
        config.curlEpsilon = 0.005f;
        config.noiseFreq = 0.8f;
        config.noiseEvolSpeed = 0.08f;
        config.fluidDrag = 0.30f;
        config.smokeDiffusion = 0.50f;
        config.masterSensitivity = 1.1f;
        config.kickImpact = 10.0f;
        config.trailDecay = 0.80f;      // tuned
        config.particleOpacity = 0.20f;  // tuned
        config.colorSaturation = 1.0f;
        config.callResponseMode = true;
        config.callResponseGravity = 80.0f;
        config.callResponseWind = 3.0f;
        config.shoegazeMode = false;
        config.symmetryN = 8;
    }
    ImGui::SameLine();
    if (ImGui::Button("Shoegaze", ImVec2(85, 24))) {
        config.particleCount = 1000000;
        config.dtScale = 0.80f;
        config.curlEpsilon = 0.016f;
        config.noiseFreq = 1.4f;
        config.noiseEvolSpeed = 0.18f;
        config.fluidDrag = 0.22f;         // very low drag for long fluid momentum sways
        config.smokeDiffusion = 0.75f;     // high smoke/ink-in-water diffusion
        config.masterSensitivity = 1.6f;
        config.kickImpact = 35.0f;
        config.trailDecay = 0.85f;         // longer trails for liquid painting
        config.particleOpacity = 0.55f;    // higher particle opacity
        config.colorSaturation = 2.4f;     // exaggerated trippy colors
        config.callResponseMode = true;
        config.callResponseGravity = 160.0f;
        config.callResponseWind = 10.0f;
        config.shoegazeMode = true;
        config.symmetryN = 8;
    }
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Simulation State", ImGuiTreeNodeFlags_DefaultOpen)) {
        int currentCount = config.particleCount;
        ImGui::SliderInt("Particle Count", &currentCount, 100000, 2000000);
        // Safely reallocate buffer only when mouse is released to prevent VRAM thrashing
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            config.particleCount = currentCount;
        }
        ImGui::SliderFloat("Simulation Speed", &config.dtScale, 0.0f, 2.0f);
        ImGui::SliderFloat("Smoke Diffusion", &config.smokeDiffusion, 0.0f, 1.5f, "%.2f");
    }

    if (ImGui::CollapsingHeader("Fractal Attractor Map", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Attractor A", &config.attractorA, -3.0f, 3.0f, "%.3f");
        ImGui::SliderFloat("Attractor B", &config.attractorB, -3.0f, 3.0f, "%.3f");
        ImGui::SliderFloat("Attractor C", &config.attractorC, -3.0f, 3.0f, "%.3f");
        ImGui::SliderFloat("Attractor D", &config.attractorD, -3.0f, 3.0f, "%.3f");
        ImGui::SliderInt("Symmetry N-Fold", &config.symmetryN, 1, 16);
        ImGui::SliderFloat("Attractor Damping", &config.fluidDrag, 0.0f, 2.0f);
    }

    if (ImGui::CollapsingHeader("Audio Gain Sensitivity", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Master Audio Sensitivity", &config.masterSensitivity, 0.0f, 15.0f);
        ImGui::SliderFloat("Kick Impact Force", &config.kickImpact, 0.0f, 100.0f);
    }

    if (ImGui::CollapsingHeader("Mouse Interaction", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Mouse Gravity Strength", &config.mouseGravity, 0.0f, 600.0f, "%.1f");
    }

    if (ImGui::CollapsingHeader("Call & Response (Drums Interaction)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable Call & Response Mode", &config.callResponseMode);
        ImGui::SliderFloat("Pulse Gravity Strength", &config.callResponseGravity, 0.0f, 500.0f, "%.1f");
        ImGui::SliderFloat("Pulse Wind Strength", &config.callResponseWind, 0.0f, 30.0f, "%.1f");
    }

    if (ImGui::CollapsingHeader("Cinematic Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Base Chevron Scale", &config.baseScale, 0.005f, 0.06f, "%.4f");
        ImGui::SliderFloat("Particle Opacity", &config.particleOpacity, 0.05f, 1.5f, "%.2f");
        ImGui::SliderFloat("Color Saturation", &config.colorSaturation, 0.5f, 2.5f, "%.2f");
        ImGui::SliderFloat("Velocity Flare Gain", &config.flareSensitivity, 0.5f, 6.0f);
        ImGui::SliderFloat("Trail Decay (Feedback)", &config.trailDecay, 0.70f, 0.98f);
        ImGui::SliderFloat("Bloom Intensity", &config.bloomIntensity, 0.0f, 4.0f);
        ImGui::SliderFloat("Exposure", &config.exposure, 0.2f, 2.5f);
    }

    // UI Restore Defaults Button
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Button("Restore Defaults", ImVec2(-1, 28))) {
        config = SimulationConfig();
    }

    ImGui::End();

    // Force absolute black clear state for all clears
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // --- STEP 1: FEEDBACK WARP PASS ---
    glBindFramebuffer(GL_FRAMEBUFFER, m_fboSceneA);
    glViewport(0, 0, m_width, m_height);
    glDisable(GL_BLEND); // Copy history directly

    m_feedbackShader.use();
    m_feedbackShader.setFloat("u_time", time);
    m_feedbackShader.setFloat("u_bass", audio.bass * config.masterSensitivity);
    m_feedbackShader.setFloat("u_mids", audio.mids * config.masterSensitivity);
    m_feedbackShader.setFloat("u_trailDecay", config.trailDecay);
    m_feedbackShader.setInt("u_N", config.symmetryN);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texSceneB);
    m_feedbackShader.setInt("u_history_texture", 0);

    glBindVertexArray(m_postProcessVao);
    drawFullScreenQuad();

    // --- STEP 2: HARDWARE-INSTANCED PARTICLE RENDER PASS ---
    // Draw 3D chevrons additively over the feedback history
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    m_particleShader.use();
    m_particleShader.setFloat("u_lightningActive", lightningActive);
    m_particleShader.setVec2("u_lightningStart", lightningStart);
    m_particleShader.setVec2("u_lightningEnd", lightningEnd);
    m_particleShader.setFloat("u_baseScale", config.baseScale);
    m_particleShader.setFloat("u_flareSensitivity", config.flareSensitivity);
    m_particleShader.setFloat("u_hype", hype);
    m_particleShader.setFloat("u_leftGravity", config.callResponseMode ? leftPulse * config.callResponseGravity : 0.0f);
    m_particleShader.setFloat("u_rightGravity", config.callResponseMode ? rightPulse * config.callResponseGravity : 0.0f);
    m_particleShader.setVec2("u_windForce", config.callResponseMode ? glm::vec2((leftPulse - rightPulse) * config.callResponseWind, 0.0f) : glm::vec2(0.0f));
    m_particleShader.setFloat("u_harshness", harshness);
    m_particleShader.setInt("u_shoegazeMode", config.shoegazeMode ? 1 : 0);
    
    // Dynamically scale particle opacity with the overall normalized volume of the track
    // We use a non-linear power curve to create a massive opacity shift between quiet verses (soft/faint)
    // and loud chorus segments (highly solid and dense).
    float masterVol = (audio.bass + audio.mids + audio.treble) / 3.0f;
    float dynamicOpacity = config.particleOpacity * (0.02f + std::pow(masterVol, 2.5f) * 0.98f);
    m_particleShader.setFloat("u_particleOpacity", dynamicOpacity);

    m_particleShader.setFloat("u_windowWidth", static_cast<float>(m_width));
    m_particleShader.setFloat("u_windowHeight", static_cast<float>(m_height));
    m_particleShader.setFloat("u_audioTime", audioTime);
    m_particleShader.setInt("u_N", config.symmetryN);
    
    // Bind audio parameters scaled by global master sensitivity
    m_particleShader.setFloat("u_bass", audio.bass * config.masterSensitivity);
    m_particleShader.setFloat("u_mids", audio.mids * config.masterSensitivity);
    m_particleShader.setFloat("u_treble", audio.treble * config.masterSensitivity);
    m_particleShader.setFloat("u_rotationAngle", rotationAngle);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSsbo);
    
    // Bind particle VAO (empty, point drawing is driven by gl_VertexID)
    glBindVertexArray(m_particleVao);
    
    // Draw particles as points with N-fold symmetry duplication
    glDrawArrays(GL_POINTS, 0, numParticles * config.symmetryN);

    // --- STEP 3: PING-PONG COPY/SWAP ---
    std::swap(m_fboSceneA, m_fboSceneB);
    std::swap(m_texSceneA, m_texSceneB);

    // --- STEP 4: BLOOM EXTRACTION (1/4 RESOLUTION) ---
    int bloomW = m_width / 4;
    int bloomH = m_height / 4;
    glBindFramebuffer(GL_FRAMEBUFFER, m_fboBloomThreshold);
    glViewport(0, 0, bloomW, bloomH);
    glDisable(GL_BLEND);
    glClear(GL_COLOR_BUFFER_BIT);

    m_bloomThresholdShader.use();
    m_bloomThresholdShader.setFloat("u_treble", audio.treble * config.masterSensitivity);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texSceneB);
    m_bloomThresholdShader.setInt("u_scene_texture", 0);

    glBindVertexArray(m_postProcessVao);
    drawFullScreenQuad();

    // --- STEP 5: MULTI-PASS GAUSSIAN BLUR (1/4 RESOLUTION) ---
    m_blurShader.use();
    int blurIterations = 3;
    unsigned int currentSourceTex = m_texBloomThreshold;
    
    for (int i = 0; i < blurIterations; ++i) {
        // Horizontal Pass: read currentSourceTex, write to m_fboBloomBlur
        glBindFramebuffer(GL_FRAMEBUFFER, m_fboBloomBlur);
        glClear(GL_COLOR_BUFFER_BIT);
        m_blurShader.setBool("u_horizontal", true);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, currentSourceTex);
        m_blurShader.setInt("u_image", 0);
        drawFullScreenQuad();
        
        // Vertical Pass: read m_texBloomBlur, write to m_fboBloomThreshold
        glBindFramebuffer(GL_FRAMEBUFFER, m_fboBloomThreshold);
        glClear(GL_COLOR_BUFFER_BIT);
        m_blurShader.setBool("u_horizontal", false);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_texBloomBlur);
        m_blurShader.setInt("u_image", 0);
        drawFullScreenQuad();
        
        currentSourceTex = m_texBloomThreshold;
    }

    // --- STEP 6: FINAL COMPOSITING PASS (FULL RESOLUTION SCREEN) ---
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_width, m_height);
    glClear(GL_COLOR_BUFFER_BIT);

    float dynamicBloom = config.bloomIntensity * (1.0f + hype * 0.5f);
    dynamicBloom = std::min(dynamicBloom, 6.0f);
    float dynamicSat = config.colorSaturation * (1.0f + hype * 0.25f);
    dynamicSat = std::min(dynamicSat, 3.0f);

    m_compositeShader.use();
    m_compositeShader.setFloat("u_bass", audio.bass * config.masterSensitivity);
    m_compositeShader.setFloat("u_bloom_intensity", dynamicBloom);
    m_compositeShader.setFloat("u_exposure", config.exposure);
    m_compositeShader.setFloat("u_colorSaturation", dynamicSat);
    m_compositeShader.setFloat("u_harshness", harshness);
    m_compositeShader.setFloat("u_windowWidth", static_cast<float>(m_width));
    m_compositeShader.setFloat("u_windowHeight", static_cast<float>(m_height));
    m_compositeShader.setFloat("u_lightningActive", lightningActive);
    m_compositeShader.setVec2("u_lightningStart", lightningStart);
    m_compositeShader.setVec2("u_lightningEnd", lightningEnd);
    m_compositeShader.setFloat("u_audioTime", audioTime);

    // Bind sharp scene
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texSceneB);
    m_compositeShader.setInt("u_scene_texture", 0);

    // Bind blurred bloom
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_texBloomThreshold);
    m_compositeShader.setInt("u_bloom_texture", 1);

    drawFullScreenQuad();

    // --- STEP 7: RENDER DEAR IMGUI OVERLAY ---
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Render ImGui viewports for multi-viewport support
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }

    // Clean up bindings
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}
