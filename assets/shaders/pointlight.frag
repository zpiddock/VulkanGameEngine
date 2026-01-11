#version 460

layout(location = 0) in vec2 fragOffset;

layout(location = 0) out vec4 outColour;

struct PointLight {

    vec4 position;
    vec4 colour;
};

layout(set = 0, binding = 0) uniform GlobalUbo {

    mat4 projectionMatrix;
    mat4 viewMatrix;
    vec4 ambientLightColour;
    PointLight pointLights[10];
    int numLights;
} ubo;

layout(push_constant) uniform Push {

    vec4 position;
    vec4 colour;
    float radius;
} push;

void main() {

    float distanceFromOffset = sqrt(dot(fragOffset, fragOffset));

    if(distanceFromOffset >= 1.0){
        discard;
    }

    outColour = vec4(push.colour.xyz, 1.0);
}
