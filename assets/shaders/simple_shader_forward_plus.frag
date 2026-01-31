#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 inColour;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorldSpace;
layout(location = 3) in vec2 fragUV;

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

// Set 2: Bindless textures and materials
layout(set = 2, binding = 0) uniform sampler2D albedoTextures[];
layout(set = 2, binding = 1) uniform sampler2D normalTextures[];
layout(set = 2, binding = 2) uniform sampler2D pbrTextures[];
layout(set = 2, binding = 3) uniform sampler2D opacityTextures[];

// Material buffer (std430 packing)
struct MaterialData {
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    uint albedoTextureIndex;
    uint normalTextureIndex;
    uint pbrTextureIndex;
    uint opacityTextureIndex;
    uint materialFlags;
    uint _padding[3];  // Pad to 64 bytes for array alignment
};

layout(set = 2, binding = 4) readonly buffer MaterialBuffer {
    MaterialData materials[];
} materialBuffer;

// Push constants
layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
    uint materialIndex;      // Index into materialBuffer.materials[]
    uvec2 tileCount;         // Forward+ tile indexing
    uint tileSize;           // Tile size in pixels
    uint maxLightsPerTile;   // Maximum lights per tile
} push;

vec3 getNormalFromMap() {
    MaterialData mat = materialBuffer.materials[push.materialIndex];

    // If no normal map, use vertex normal
    if ((mat.materialFlags & 2u) == 0u) {
        return normalize(fragNormalWorldSpace);
    }

    // Sample normal map
    vec3 tangentNormal = texture(normalTextures[nonuniformEXT(mat.normalTextureIndex)], fragUV).xyz * 2.0 - 1.0;
    tangentNormal.xy *= mat.normalScale;

    // Derive TBN matrix from derivatives
    vec3 Q1 = dFdx(fragPosWorld);
    vec3 Q2 = dFdy(fragPosWorld);
    vec2 st1 = dFdx(fragUV);
    vec2 st2 = dFdy(fragUV);

    vec3 N = normalize(fragNormalWorldSpace);
    vec3 T = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

void main() {
    MaterialData mat = materialBuffer.materials[push.materialIndex];

    // Sample albedo with alpha
    vec4 albedoSample = vec4(1.0);
    if ((mat.materialFlags & 1u) != 0u) {
        albedoSample = texture(albedoTextures[nonuniformEXT(mat.albedoTextureIndex)], fragUV);
    }
    vec3 albedo = inColour * mat.baseColorFactor.rgb * albedoSample.rgb;
    float alpha = mat.baseColorFactor.a * albedoSample.a;

    // Sample opacity texture if present (multiplies with existing alpha)
    if ((mat.materialFlags & 8u) != 0u) {  // bit 3 = has_opacity
        float opacitySample = texture(opacityTextures[nonuniformEXT(mat.opacityTextureIndex)], fragUV).r;
        alpha *= opacitySample;  // Use opacity directly: white=opaque, black=transparent
    }

    // Discard fully transparent fragments AND very low alpha materials
    // Materials with alpha < 0.15 (like Eyewetness at 0.1) cause darkening even with specular-only
    if (alpha < 0.15) {
        discard;
    }

    // Sample PBR properties
    float metallic = mat.metallicFactor;
    float roughness = mat.roughnessFactor;
    if ((mat.materialFlags & 4u) != 0u) {
        vec2 pbr = texture(pbrTextures[nonuniformEXT(mat.pbrTextureIndex)], fragUV).rg;
        metallic *= pbr.r;
        roughness *= pbr.g;
    }

    // Get normal (with normal mapping if available)
    vec3 N = getNormalFromMap();
    vec3 V = normalize(ubo.inverseViewMatrix[3].xyz - fragPosWorld);

    vec3 diffuseLight = ubo.ambientLightColour.xyz * ubo.ambientLightColour.w;
    vec3 specularLight = vec3(0.0);

    // Forward+ tile-based lighting
    uvec2 tileID = uvec2(gl_FragCoord.xy) / push.tileSize;
    uint tileIndex = tileID.y * push.tileCount.x + tileID.x;
    uint lightsInTile = lightCount.data[tileIndex];

    // Iterate over lights in this tile only
    uint baseOffset = tileIndex * push.maxLightsPerTile;
    for (uint i = 0; i < lightsInTile; ++i) {
        uint lightIndex = lightGrid.data[baseOffset + i];

        if (lightIndex >= ubo.numLights) continue;

        PointLight light = ubo.pointLights[lightIndex];
        vec3 L = light.position.xyz - fragPosWorld;
        float attenuation = 1.0 / dot(L, L);
        L = normalize(L);

        float NdotL = max(dot(N, L), 0.0);
        vec3 intensity = light.colour.xyz * light.colour.w * attenuation;

        diffuseLight += intensity * NdotL;

        // Specular using Blinn-Phong (simplified PBR)
        vec3 H = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), mix(256.0, 32.0, roughness));
        specularLight += intensity * spec * mix(0.04, 1.0, metallic);
    }

    // For very low-alpha materials (< 0.2), render as specular-only glossy layer
    // This prevents semi-transparent gray layers from darkening underlying geometry
    vec3 finalColor;
    if (alpha < 0.2) {
        // Pure specular - no diffuse contribution at all
        finalColor = specularLight;
    } else {
        // Normal PBR shading
        finalColor = diffuseLight * albedo + specularLight;
    }
    outColour = vec4(finalColor, alpha);
}
