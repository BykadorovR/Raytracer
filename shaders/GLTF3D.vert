#version 450

layout(set = 0, binding = 0) uniform UniformCamera {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

layout(std430, set = 2, binding = 0) readonly buffer JointMatrices {
    mat4 jointMatrices[];
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec4 jointIndices;
layout(location = 5) in vec4 jointWeights;
layout(location = 6) in vec4 tangent;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

layout( push_constant ) uniform constants {
    int jointNum;
} PushConstants;

void main() {
    mat4 skinMat = mat4(1.0);
    if (PushConstants.jointNum > 0) {
        skinMat = jointWeights.x * jointMatrices[int(jointIndices.x)] +
                  jointWeights.y * jointMatrices[int(jointIndices.y)] +
                  jointWeights.z * jointMatrices[int(jointIndices.z)] +
                  jointWeights.w * jointMatrices[int(jointIndices.w)];
    }

    gl_Position = mvp.proj * mvp.view * mvp.model * skinMat * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}