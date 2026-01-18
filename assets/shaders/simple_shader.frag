#version 460

layout(location = 0) in vec3 inColour;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorldSpace;

layout(location = 0) out vec4 outColour;

struct PointLight {

    vec4 position;
    vec4 colour;
};

layout(set = 0, binding = 0) uniform GlobalUbo {

    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseViewMatrix;
    vec4 ambientLightColour;
    PointLight pointLights[10];
    int numLights;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix; // projection * view * model
    mat4 normalMatrix;
} push;

void main() {

    vec3 diffuseLight = ubo.ambientLightColour.xyz * ubo.ambientLightColour.w;
    vec3 specularLight = vec3(0.0);
    vec3 surfaceNormal = normalize(fragNormalWorldSpace);

    vec3 cameraWorldPos = ubo.inverseViewMatrix[3].xyz;
    vec3 viewDirection = normalize(cameraWorldPos - fragPosWorld);

    for(int i = 0; i < ubo.numLights; i++) {

        PointLight light = ubo.pointLights[i];
        vec3 directionToLight = light.position.xyz - fragPosWorld;
        float attenutation = 1.0 / dot(directionToLight, directionToLight);

        directionToLight = normalize(directionToLight);

        float cosAngIncidence = max(dot(surfaceNormal, directionToLight), 0);
        vec3 intensity = light.colour.xyz * light.colour.w * attenutation;

        diffuseLight += intensity * cosAngIncidence;

        // Specular
        vec3 halfAngle = normalize(directionToLight + viewDirection);
        float blinnTerm = dot(surfaceNormal, halfAngle);
        blinnTerm = clamp(blinnTerm, 0, 1);
        blinnTerm = pow(blinnTerm, 32.0); // Higher value = sharper higlights

        specularLight += intensity * blinnTerm;
    }

    outColour = vec4(diffuseLight * inColour + specularLight * inColour, 1.0);
}
