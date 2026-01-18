#version 460

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

// G-buffer textures
layout(set = 1, binding = 0) uniform sampler2D gbufferPosition;
layout(set = 1, binding = 1) uniform sampler2D gbufferNormal;
layout(set = 1, binding = 2) uniform sampler2D gbufferAlbedo;

struct PointLight {
    vec4 position;
    vec4 colour;
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseViewMartix;
    vec4 ambientLightColour;
    PointLight pointLights[10];
    int numLights;
} ubo;

void main() {
    // Sample G-buffer
    vec3 position = texture(gbufferPosition, fragUV).xyz;
    vec3 normal = normalize(texture(gbufferNormal, fragUV).xyz);
    vec3 albedo = texture(gbufferAlbedo, fragUV).rgb;

    // Get camera position from inverse view matrix
    vec3 cameraPos = ubo.inverseViewMartix[3].xyz;
    vec3 viewDir = normalize(cameraPos - position);

    // Ambient lighting
    vec3 ambientLight = ubo.ambientLightColour.xyz * ubo.ambientLightColour.w;

    // Accumulate diffuse and specular lighting from point lights
    vec3 diffuseLight = vec3(0.0);
    vec3 specularLight = vec3(0.0);

    for(int i = 0; i < ubo.numLights; i++) {
        PointLight light = ubo.pointLights[i];
        vec3 lightDir = light.position.xyz - position;
        float distance = length(lightDir);
        lightDir = normalize(lightDir);

        // Attenuation
        float attenuation = 1.0 / dot(lightDir * distance, lightDir * distance);
        vec3 lightIntensity = light.colour.xyz * light.colour.w * attenuation;

        // Diffuse (Lambertian)
        float diffuseFactor = max(dot(normal, lightDir), 0.0);
        diffuseLight += lightIntensity * diffuseFactor;

        // Specular (Blinn-Phong)
        if (diffuseFactor > 0.0) {
            vec3 halfwayDir = normalize(lightDir + viewDir);
            float specularFactor = pow(max(dot(normal, halfwayDir), 0.0), 32.0); // 32 = shininess
            specularLight += lightIntensity * specularFactor;
        }
    }

    // Combine lighting
    vec3 finalColor = (ambientLight + diffuseLight) * albedo + specularLight;
    outColor = vec4(finalColor, 1.0);
}
