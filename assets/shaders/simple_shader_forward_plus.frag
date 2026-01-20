#version 460

layout(location = 0) in vec3 inColour;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorldSpace;

layout(location = 0) out vec4 outColour;

struct PointLight {
    vec4 position;
    vec4 colour;
};

// Set 0: Global UBO (same as before)
layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseViewMatrix;
    vec4 ambientLightColour;
    PointLight pointLights[10];
    int numLights;
} ubo;

// Set 1: Forward+ light grid resources
// NOTE: Binding 0 is depth texture (compute only), so fragment shader starts at binding 1
layout(set = 1, binding = 1, std430) readonly buffer LightGrid {
    uint data[];
} lightGrid;

layout(set = 1, binding = 2, std430) readonly buffer LightCount {
    uint data[];
} lightCount;

// Push constants
layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
    uvec2 tileCount;         // NEW: For Forward+ tile indexing
    uint tileSize;           // NEW: Tile size in pixels
    uint maxLightsPerTile;   // NEW: Maximum lights per tile
} push;

void main() {
    vec3 diffuseLight = ubo.ambientLightColour.xyz * ubo.ambientLightColour.w;
    vec3 specularLight = vec3(0.0);
    vec3 surfaceNormal = normalize(fragNormalWorldSpace);

    vec3 cameraWorldPos = ubo.inverseViewMatrix[3].xyz;
    vec3 viewDirection = normalize(cameraWorldPos - fragPosWorld);

    // Forward+ tile-based lighting
    // Compute tile index from fragment position
    uvec2 tileID = uvec2(gl_FragCoord.xy) / push.tileSize;
    uint tileIndex = tileID.y * push.tileCount.x + tileID.x;

    // Get light count for this tile
    uint lightsInTile = lightCount.data[tileIndex];

    // Iterate over lights in this tile only (Forward+ optimization!)
    uint baseOffset = tileIndex * push.maxLightsPerTile;
    for (uint i = 0; i < lightsInTile; ++i) {
        uint lightIndex = lightGrid.data[baseOffset + i];

        // Bounds check (safety)
        if (lightIndex >= ubo.numLights) continue;

        PointLight light = ubo.pointLights[lightIndex];
        vec3 directionToLight = light.position.xyz - fragPosWorld;
        float attenuation = 1.0 / dot(directionToLight, directionToLight);

        directionToLight = normalize(directionToLight);

        float cosAngIncidence = max(dot(surfaceNormal, directionToLight), 0);
        vec3 intensity = light.colour.xyz * light.colour.w * attenuation;

        diffuseLight += intensity * cosAngIncidence;

        // Blinn-Phong specular
        vec3 halfAngle = normalize(directionToLight + viewDirection);
        float blinnTerm = dot(surfaceNormal, halfAngle);
        blinnTerm = clamp(blinnTerm, 0, 1);
        blinnTerm = pow(blinnTerm, 32.0);

        specularLight += intensity * blinnTerm;
    }

    outColour = vec4(diffuseLight * inColour + specularLight * inColour, 1.0);
}
