#version 450

// Vertex input
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

// UBO with camera matrices
layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    vec4 ambientLightColor;
    // ... light data not used in depth pre-pass
} ubo;

// Push constants for model matrix
layout(push_constant) uniform PushConstants {
    mat4 modelMatrix;
    mat4 normalMatrix;
} push;

void main() {
    // Transform vertex position to clip space
    vec4 positionWorld = push.modelMatrix * vec4(position, 1.0);
    gl_Position = ubo.projection * ubo.view * positionWorld;
}
