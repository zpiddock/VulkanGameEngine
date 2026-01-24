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
    mat4 inverseViewMatrix;
    vec4 ambientLightColour;
    PointLight pointLights[10];
    int numLights;
} ubo;

layout(push_constant) uniform Push {

    vec4 position;
    vec4 colour;
    float radius;
} push;

const float M_PI = 3.1415926538;

void main() {

    float distanceFromOffset = sqrt(dot(fragOffset, fragOffset));

    if(distanceFromOffset >= 1.0){
        discard;
    }

    float cosDis = 0.5 * (cos(distanceFromOffset * M_PI) + 1.0);

    // Premultiplied alpha: multiply color by falloff, blend mode will just add (no alpha multiplication)
    // This creates a smooth bloom-like glow with no dark edges
    outColour = vec4(push.colour.xyz * cosDis, 1.0);
}
