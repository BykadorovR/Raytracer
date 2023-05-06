#version 450

layout(set = 0, binding = 0) uniform UniformCamera {
    mat4 model;
} mvp;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec3 inTangent;

layout(location = 0) out vec2 fragTexCoord;

void main() {
    gl_Position = mvp.model * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
}