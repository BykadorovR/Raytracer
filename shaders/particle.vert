#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec3 fragColor;

layout(set = 0, binding = 0) uniform UniformCamera {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

void main() {
    gl_PointSize = 14.0;
    gl_Position = mvp.proj * mvp.view * mvp.model * vec4(inPosition.xyz, 1.0);
    fragColor = inColor.rgb;
}