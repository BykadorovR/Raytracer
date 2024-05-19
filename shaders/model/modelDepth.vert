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

layout(location = 0) out vec4 modelCoords;

layout(std430, set = 1, binding = 0) readonly buffer JointMatrices {
    int jointNumber;
    mat4 jointMatrices[];
};

void main() {
    mat4 skinMat = mat4(1.0);
    if (jointNumber > 0) {
        skinMat = inJointWeights.x * jointMatrices[int(inJointIndices.x)] +
                  inJointWeights.y * jointMatrices[int(inJointIndices.y)] +
                  inJointWeights.z * jointMatrices[int(inJointIndices.z)] +
                  inJointWeights.w * jointMatrices[int(inJointIndices.w)];
    }

    mat4 model = mvp.model * skinMat;
    gl_Position = mvp.proj * mvp.view * model * vec4(inPosition, 1.0);
    modelCoords = model * vec4(inPosition, 1.0);
}