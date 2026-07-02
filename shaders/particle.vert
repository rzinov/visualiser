#version 430 core

struct Particle {
    vec4 position_life; // xy = position, z = depth/species, w = life (0.0 to 1.0)
    vec4 velocity_mass; // xyz = velocity, w = mass
};

layout(std430, binding = 0) readonly buffer ParticleBuffer {
    Particle particles[];
};

out float v_life;
out vec2 v_velocity;
out vec3 v_worldPos;
out float v_species;
out float v_ionization;

uniform float u_baseScale;
uniform float u_treble;
uniform float u_mids;
uniform float u_audioTime;
uniform float u_windowWidth;
uniform float u_windowHeight;
uniform int u_N;
uniform float u_rotationAngle;

uniform float u_lightningActive;
uniform vec2 u_lightningStart;
uniform vec2 u_lightningEnd;
uniform float u_hype;
uniform float u_leftGravity;
uniform float u_rightGravity;
uniform vec2 u_windForce;
uniform float u_harshness;

void main() {
    // Symmetry Duplication: Map vertex ID to particle and symmetry index
    int pIdx = gl_VertexID / u_N;
    int sIdx = gl_VertexID % u_N;

    Particle p = particles[pIdx];

    float species = p.position_life.z;
    v_species = species;
    v_life = p.position_life.w;
    v_velocity = p.velocity_mass.xy;
    v_ionization = p.velocity_mass.z;

    vec2 pos = p.position_life.xy * 0.45;
    float r = length(pos);
    float theta = atan(pos.y, pos.x);
    
    float rotationOffset = u_rotationAngle + (u_mids + u_treble) * 0.08;
    float symTheta = theta + (2.0 * 3.14159265359 * float(sIdx)) / float(u_N) + rotationOffset;
    vec2 symPos = vec2(r * cos(symTheta), r * sin(symTheta));

    // Dynamic point size scaling: double-sided life envelope
    float resScale = min(u_windowWidth, u_windowHeight) / 1080.0;
    float lifeScale = smoothstep(0.0, 0.15, v_life) * smoothstep(1.0, 0.85, v_life);

    // Dual-Scale Acoustic Ripples: micro-frequency phase shift to the position
    // driven by mids/treble transients
    float phaseFreq = 380.0;
    float phaseSpeed = 35.0;
    float phaseAmp = 0.0006 * (u_treble * 1.5 + u_mids * 0.5) * lifeScale;
    vec2 phaseOffset = vec2(
        sin(symPos.y * phaseFreq - u_audioTime * phaseSpeed),
        cos(symPos.x * phaseFreq - u_audioTime * phaseSpeed)
    ) * phaseAmp;
    
    symPos += phaseOffset;

    // Apply screen-space call and response wind displacement (parabolic wind profile)
    float windDisp = u_windForce.x * (1.0 - symPos.y * symPos.y * 0.25) * lifeScale * 0.03;
    symPos.x += windDisp;

    float aspect = u_windowWidth / u_windowHeight;

    // Apply screen-space call and response gravity attraction pulls
    if (u_leftGravity > 0.0) {
        vec2 toLeft = vec2(-0.8 * aspect, 0.0) - symPos;
        float dist = length(toLeft);
        if (dist > 0.01) {
            symPos += normalize(toLeft) * (u_leftGravity * 0.0003) * lifeScale / (dist + 0.2);
        }
    }
    if (u_rightGravity > 0.0) {
        vec2 toRight = vec2(0.8 * aspect, 0.0) - symPos;
        float dist = length(toRight);
        if (dist > 0.01) {
            symPos += normalize(toRight) * (u_rightGravity * 0.0003) * lifeScale / (dist + 0.2);
        }
    }

    v_worldPos = vec3(symPos, 0.0);

    gl_Position = vec4(symPos.x / aspect, symPos.y, 0.0, 1.0);

    // Modulate point size by treble/mids transient octave (allow massive scaling for epic visual peaks)
    float acousticSizeScale = 1.0 + clamp(u_treble * 3.5 + u_mids * 1.5, 0.0, 10.0) * lifeScale;
    
    float speciesScale = 1.0;
    if (species < 0.5) {
        speciesScale = 15.0; // Bass particles are big and soft
    } else if (species < 1.5) {
        speciesScale = 6.0;  // Mids are medium size
    } else {
        speciesScale = 2.8;  // Treble are small and numerous
    }
    
    float basePixelSize = u_baseScale * u_windowHeight;
    gl_PointSize = basePixelSize * speciesScale * resScale * lifeScale * acousticSizeScale * (1.0 + u_hype * 0.35);
    
    // Scale up size of ionized/lightning particles dynamically
    if (v_ionization > 0.01) {
        gl_PointSize *= (1.0 + v_ionization * 1.5);
    }
    
    gl_PointSize = clamp(gl_PointSize, 1.0, 256.0);
}
