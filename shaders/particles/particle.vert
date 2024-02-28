#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float maxLife;
layout(location = 3) in float maxVelocity;
layout(location = 4) in vec3 velocityDirection;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform UniformCamera {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

layout( push_constant ) uniform constants {
    float pointScale;
} push;

void main() {
    vec4 viewPosition = mvp.view * mvp.model * vec4(inPosition.xyz, 1.0);
    gl_Position = mvp.proj * viewPosition;
    //maxVelocity * velocityDirection * maxLife - is maximum point particle can reach, average start position is (0, 0, 0)
    gl_PointSize = distance(inPosition, (maxVelocity * velocityDirection * maxLife)) * push.pointScale / abs(viewPosition.z);
    fragColor = inColor;
}