#version 450

layout(binding = 0) uniform UniformCamera {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    gl_Position = vec4(fragTexCoord * 2.0f - 1.0f, 0.0f, 1.0f);
}