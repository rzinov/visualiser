#version 430 core

in float v_life;
in vec2 v_velocity;
in vec3 v_worldPos;
in float v_species;
in float v_ionization;

out vec4 FragColor;

uniform float u_bass;
uniform float u_mids;
uniform float u_treble;
uniform float u_flareSensitivity;
uniform float u_audioTime;
uniform float u_particleOpacity;
uniform float u_harshness;
uniform int u_shoegazeMode;

// Standard HSV to RGB converter
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    // For GL_POINTS, we use gl_PointCoord (goes from 0.0 to 1.0)
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);
    
    if (dist > 0.5) {
        discard;
    }

    // Soft radial falloff for vapor splatting
    float alphaFactor = smoothstep(0.5, 0.0, dist);
    alphaFactor = pow(alphaFactor, 2.2);

    // Calculate spherical normal for volumetric lighting
    vec2 nXY = coord * 2.0;
    float nz2 = 1.0 - dot(nXY, nXY);
    vec3 normal = vec3(nXY, (nz2 > 0.0) ? sqrt(nz2) : 0.0);
    normal = normalize(normal);

    // Soft Half-Lambert diffuse light shading
    vec3 lightDir = normalize(vec3(0.35, 0.45, 0.82));
    float diffuse = clamp(dot(normal, lightDir) * 0.45 + 0.55, 0.0, 1.0);

    float speed = length(v_velocity);
    float speedFactor = clamp(speed * u_flareSensitivity, 0.0, 1.0);

    // Color gradient interpolation factor (head to tail / center to edge)
    float t = 1.0 - (dist * 2.0);
    t = clamp(t, 0.0, 1.0);

    // HSL/HSV Dynamic Color Wheel Harmonies
    float hueBase = u_audioTime * 0.06;
    vec3 finalColor = vec3(0.0);
    float alpha = 1.0;

    // Smooth double-sided birth-death life envelope
    float lifeFade = smoothstep(0.0, 0.15, v_life) * smoothstep(1.0, 0.85, v_life);

    if (u_shoegazeMode == 1) {
        // Exaggerated trippy colors for Shoegaze:
        // Use a continuous rotating HSV cycle shifted per species to create trippy rainbow sways
        float h = hueBase + v_species * 0.22;
        // Maximum saturation (1.0) and high brightness (1.4) for neon trippy colors
        vec3 headColor = hsv2rgb(vec3(h, 1.0, 1.4));
        vec3 tailColor = hsv2rgb(vec3(h + 0.12, 1.0, 0.22));
        vec3 baseColor = mix(tailColor, headColor, t);
        
        float speedGlow = 0.5 + speedFactor * 0.5;
        finalColor = baseColor * diffuse * speedGlow;
        // Strongly amplify color reaction to real-time audio bands
        float audioBoost = (v_species < 0.5) ? u_bass : ((v_species < 1.5) ? u_mids : u_treble);
        finalColor += headColor * clamp(audioBoost, 0.0, 3.5) * 0.65 * diffuse * t;
        
        // Exaggerated opacity to make it look like solid thick liquid sheets
        float baseAlpha = (v_species < 0.5) ? 0.95 : ((v_species < 1.5) ? 0.65 : 0.35);
        alpha = lifeFade * baseAlpha * alphaFactor;
        // Extra boost based on velocity flare
        alpha *= (1.0 + speedFactor * 0.8);
    }
    else {
        if (v_species < 0.5) {
            // Species 0: Bass (Base Hue)
            float h = hueBase;
            vec3 headColor = hsv2rgb(vec3(h, 0.95, 1.0));
            vec3 tailColor = hsv2rgb(vec3(h + 0.04, 0.90, 0.15));
            vec3 baseColor = mix(tailColor, headColor, t);
            
            float speedGlow = 0.5 + speedFactor * 0.5;
            finalColor = baseColor * diffuse * speedGlow;
            finalColor += headColor * clamp(u_bass, 0.0, 3.5) * 0.45 * diffuse * t;
            
            alpha = lifeFade * 0.95 * alphaFactor;
        }
        else if (v_species < 1.5) {
            // Species 1: Mids (Triadic 120-degree shift)
            float h = hueBase + 0.333;
            vec3 headColor = hsv2rgb(vec3(h, 0.90, 1.0));
            vec3 tailColor = hsv2rgb(vec3(h + 0.04, 0.85, 0.15));
            vec3 baseColor = mix(tailColor, headColor, t);
            
            float speedGlow = 0.4 + speedFactor * 0.6;
            finalColor = baseColor * diffuse * speedGlow;
            finalColor += headColor * clamp(u_mids, 0.0, 3.5) * 0.35 * diffuse * t;
            
            alpha = (0.15 + speedFactor * 0.35) * lifeFade * 0.60 * alphaFactor;
        }
        else {
            // Species 2: Treble Swarm (Triadic 240-degree shift)
            float h = hueBase + 0.667;
            vec3 headColor = hsv2rgb(vec3(h, 0.85, 1.0));
            if (speedFactor > 0.6) {
                // Keep colors highly saturated rather than washing them out to pure white
                headColor = mix(headColor, hsv2rgb(vec3(h, 0.80, 1.0)), (speedFactor - 0.6) * 0.5);
            }
            vec3 tailColor = hsv2rgb(vec3(h + 0.04, 0.80, 0.12));
            vec3 baseColor = mix(tailColor, headColor, t);
            
            float speedGlow = 0.35 + speedFactor * 0.65;
            finalColor = baseColor * diffuse * speedGlow;
            // Subtle treble color reactive glow
            finalColor += headColor * clamp(u_treble, 0.0, 3.5) * 0.30 * diffuse * t;
            
            alpha = (0.04 + speedFactor * 0.08) * lifeFade * 0.25 * alphaFactor;
        }
    }

    // Apply per-particle ionization cooling blend (white core / cyan edges)
    if (v_ionization > 0.01) {
        vec3 electricColor = mix(vec3(0.0, 1.0, 1.0), vec3(1.0, 1.0, 1.0), t);
        finalColor = mix(finalColor, electricColor, v_ionization);
        
        // Ionized particles should be bright and visible
        float targetAlpha = 0.95 * alphaFactor * lifeFade;
        alpha = mix(alpha, targetAlpha, v_ionization);
    }



    // Apply global particle opacity multiplier
    alpha *= u_particleOpacity;

    FragColor = vec4(finalColor, alpha);
}
