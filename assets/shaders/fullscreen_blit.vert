#version 460

// Fullscreen triangle vertex shader
// Generates a fullscreen triangle without vertex buffer
// Triangle covers entire screen with UVs for sampling

layout(location = 0) out vec2 fragTexCoord;

void main() {
    // Generate fullscreen triangle vertices
    // Vertex 0: (-1, -1) -> UV (0, 0)
    // Vertex 1: ( 3, -1) -> UV (2, 0)
    // Vertex 2: (-1,  3) -> UV (0, 2)
    // This oversized triangle covers the entire screen when clipped

    vec2 vertices[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    vec2 uvs[3] = vec2[](
        vec2(0.0, 0.0),
        vec2(2.0, 0.0),
        vec2(0.0, 2.0)
    );

    gl_Position = vec4(vertices[gl_VertexIndex], 0.0, 1.0);
    fragTexCoord = uvs[gl_VertexIndex];
}
