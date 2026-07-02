#version 430 core

in vec2 TexCoords;
out vec4 FragColor;

// Previous frame texture
uniform sampler2D u_history_texture;
uniform float u_time;
uniform float u_bass;
uniform float u_mids;
uniform float u_trailDecay;

void main() {
    vec2 uv = TexCoords;
    vec2 center = vec2(0.5);
    vec2 toCenter = uv - center;

    // Clamping audio inputs to maintain smooth liquid feedback even at high sensitivity
    float safe_bass = clamp(u_bass, 0.0, 10.0);
    float safe_mids = clamp(u_mids, 0.0, 10.0);

    // 1. Zoom out slightly (scaling UVs from center).
    // Bass spikes compress the screen space, causing trails to expand outward.
    float zoom = 0.996 - (safe_bass * 0.005);
    uv = center + toCenter * zoom;

    // 2. Swirling/wavy fluid displacement using sine waves modulated by music
    float warpFreq = 8.0 + safe_mids * 8.0;
    float warpAmp = 0.0012 + (safe_bass * 0.004);
    
    uv.x += sin(uv.y * warpFreq + u_time * 1.5) * warpAmp;
    uv.y += cos(uv.x * warpFreq + u_time * 1.5) * warpAmp;

    // 3. Sample history
    vec4 history = texture(u_history_texture, uv);

    // 4. Decay trails over time (feedback factor from uniform, allow up to 0.98 for long persistence)
    float decay = clamp(u_trailDecay, 0.70, 0.98);
    vec4 finalColor = history * decay;

    // Volumetric Contrast Calibration: micro-subtractive self-shadowing based on mid-tone luminance attenuation
    // Calculates luminance and subtracts a shadow factor peaking in mid-density edges (ambient occlusion approximation)
    float luma = dot(finalColor.rgb, vec3(0.2126, 0.7152, 0.0722));
    float edgeShadow = clamp(luma * (1.0 - luma) * 4.0, 0.0, 1.0);
    finalColor.rgb -= vec3(edgeShadow * 0.008);
    finalColor.rgb = max(finalColor.rgb, vec3(0.0));

    // Fade color towards a deep indigo/violet on decay to add visual richness
    // On bass hits, shift color hue slightly to create a trippy rainbow trailing effect
    vec3 colorShift = vec3(0.96, 0.92 + u_bass * 0.04, 1.0);
    finalColor.rgb *= colorShift; 

    // Safe VRAM clamp: Cap trail accumulation values directly at 1.5.
    // This physically prevents infinite exponential feedback blowout, keeping visual decay instant
    // when the music pauses while preserving saturated, vibrant HDR color shifts.
    finalColor.rgb = clamp(finalColor.rgb, 0.0, 1.5);

    // Hard threshold to clean up floating point noise in empty space
    if (length(finalColor.rgb) < 0.015) {
        finalColor = vec4(0.0, 0.0, 0.0, 0.0);
    }

    FragColor = finalColor;
}
