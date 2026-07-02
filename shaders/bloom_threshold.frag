#version 430 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D u_scene_texture;
uniform float u_treble;

void main() {
    vec3 color = texture(u_scene_texture, TexCoords).rgb;
    
    // Calculate luminance using standard relative coefficients
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    
    // Soft-knee thresholding to prevent blocky/aliased bloom extraction
    float threshold = 0.65;
    float knee = 0.25; // range of the soft-knee
    
    // Soft threshold formula
    float rq = clamp(luma - threshold + knee, 0.0, 2.0 * knee);
    rq = rq * rq / (4.0 * knee + 0.0001);
    float factor = max(rq, luma - threshold) / max(luma, 0.0001);
    
    vec3 bloomColor = color * factor;
    
    // Dynamic boost on treble spikes (clamped to prevent blowout)
    float boost = 1.0 + clamp(u_treble, 0.0, 10.0) * 0.5;
    FragColor = vec4(bloomColor * boost, 1.0);
}
