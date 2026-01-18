#version 460

layout(location = 0) in vec3 fragColour;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;

// G-buffer outputs (multiple render targets)
layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;

void main() {
    outPosition = vec4(fragPosWorld, 1.0);
    outNormal = vec4(normalize(fragNormalWorld), 1.0);  // .w reserved for roughness (PBR future)
    outAlbedo = vec4(fragColour, 1.0);  // .w reserved for AO (PBR future)
}
