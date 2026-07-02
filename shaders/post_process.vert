#version 430 core

out vec2 TexCoords;

void main() {
    // Generates a single giant triangle covering the screen using vertex ID
    // Vertex 0: (-1.0, -1.0), UV: (0.0, 0.0)
    // Vertex 1: ( 3.0, -1.0), UV: (2.0, 0.0)
    // Vertex 2: (-1.0,  3.0), UV: (0.0, 2.0)
    float x = -1.0 + float((gl_VertexID & 1) << 2);
    float y = -1.0 + float((gl_VertexID & 2) << 1);
    
    TexCoords = vec2(x * 0.5 + 0.5, y * 0.5 + 0.5);
    gl_Position = vec4(x, y, 0.0, 1.0);
}
