#version 460

// Fullscreen blit fragment shader
// Samples from offscreen color buffer and outputs to backbuffer

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

// Offscreen color buffer to sample from
layout(set = 0, binding = 0) uniform sampler2D offscreenTexture;

void main() {
    // Simple passthrough blit - sample offscreen texture
    outColor = texture(offscreenTexture, fragTexCoord);
}
