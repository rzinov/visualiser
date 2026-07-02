#version 430 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D u_scene_texture;
uniform sampler2D u_bloom_texture;

uniform float u_bass;
uniform float u_bloom_intensity;
uniform float u_exposure;
uniform float u_colorSaturation;
uniform float u_harshness;
uniform float u_windowWidth;
uniform float u_windowHeight;

uniform float u_lightningActive;
uniform vec2 u_lightningStart;
uniform vec2 u_lightningEnd;
uniform float u_audioTime;

// ACES Filmic Tone Mapping approximation
vec3 acesFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec2 toCenter = TexCoords - vec2(0.5);
    float dist = length(toCenter);

    // Chromatic Aberration: split color channels radially from screen center
    // We add a tiny baseline distortion that scales up dramatically during heavy bass transients
    float aberrationShift = 0.0015 + dist * 0.015 * u_bass;
    
    float r = texture(u_scene_texture, TexCoords + toCenter * aberrationShift).r;
    float g = texture(u_scene_texture, TexCoords).g;
    float b = texture(u_scene_texture, TexCoords - toCenter * aberrationShift).b;
    
    vec3 sceneColor = vec3(r, g, b);
    vec3 bloomColor = texture(u_bloom_texture, TexCoords).rgb;
    
    // Additive bloom mixing
    vec3 color = sceneColor + bloomColor * u_bloom_intensity;
    
    // 1. Color Saturation Adjustment
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, u_colorSaturation);
    
    // 2. Exposure scale and ACES Filmic Tone Mapping (retains rich saturation at high intensities)
    vec3 colorExposed = color * u_exposure;
    vec3 mapped = acesFilm(colorExposed);
    
    // 3. Gamma Correction (2.2 standard)
    mapped = pow(mapped, vec3(1.0 / 2.2));
    
    // 5. Procedural Glowing Lightning Bolt (mapped to NDC space [-1.0, 1.0])
    if (u_lightningActive > 0.0) {
        vec2 ndcPos = (TexCoords - vec2(0.5)) * 2.0;

        vec2 ab = u_lightningEnd - u_lightningStart;
        vec2 ap = ndcPos - u_lightningStart;
        float t_line = clamp(dot(ap, ab) / dot(ab, ab), 0.0, 1.0);
        vec2 closestPoint = u_lightningStart + t_line * ab;
        
        // Jagged line displacement using high-frequency sine/cosine waves
        float jag = sin(ndcPos.y * 45.0 + u_audioTime * 15.0) * 0.02 * (1.0 - t_line * 0.3) + cos(ndcPos.x * 30.0) * 0.015;
        vec2 jagOffset = vec2(jag, -jag * 0.5);
        float distToBolt = length(ndcPos - closestPoint + jagOffset);
        
        // Sharp inner white/cyan core
        float boltCore = smoothstep(0.007, 0.0, distToBolt);
        // Soft outer glowing aura
        float boltGlow = smoothstep(0.08, 0.0, distToBolt);
        
        vec3 electricColor = vec3(0.5, 0.9, 1.0); // electric hot cyan
        vec3 boltOutput = electricColor * (boltCore * 1.6 + boltGlow * 0.5) * u_lightningActive;
        
        mapped += boltOutput;
    }
    
    FragColor = vec4(mapped, 1.0);
}
