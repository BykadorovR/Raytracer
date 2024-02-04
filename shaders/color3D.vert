#version 450

layout(set = 0, binding = 0) uniform UniformCamera {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec4 inJointIndices;
layout(location = 5) in vec4 inJointWeights;
layout(location = 6) in vec4 inTangent;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 texCoords;

void main() {
    vec4 afterModel = mvp.model * vec4(inPosition, 1.0);
    mat3 normalMatrix = mat3(transpose(inverse(mvp.model)));

    fragColor = inColor;
    texCoords = inTexCoord;
    gl_Position = mvp.proj * mvp.view * mvp.model * vec4(inPosition, 1.0);
}  