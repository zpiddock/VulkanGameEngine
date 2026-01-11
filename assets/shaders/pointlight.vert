#version 460

const vec2 OFFSETS[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, 1.0)
);

layout(location = 0) out vec2 fragOffset;

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

    fragOffset = OFFSETS[gl_VertexIndex];

    vec3 cameraRightWorldspace = {ubo.viewMatrix[0][0], ubo.viewMatrix[1][0], ubo.viewMatrix[2][0]};
    vec3 cameraUpWorldspace = {ubo.viewMatrix[0][1], ubo.viewMatrix[1][1], ubo.viewMatrix[2][1]};

    vec3 positionWorldspace = push.position.xyz
    + push.radius * fragOffset.x * cameraRightWorldspace
    + push.radius * fragOffset.y * cameraUpWorldspace;

    gl_Position = ubo.projectionMatrix * ubo.viewMatrix * vec4(positionWorldspace, 1.0);
}
