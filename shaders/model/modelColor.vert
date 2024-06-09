#version 450

layout(set = 0, binding = 0) uniform UniformCamera {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inJointIndices;
layout(location = 4) in vec4 inJointWeights;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 texCoords;

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
    
    vec4 afterModel = model * vec4(inPosition, 1.0);
    mat3 normalMatrix = mat3(transpose(inverse(model)));

    fragColor = inColor;
    texCoords = inTexCoord;
    gl_Position = mvp.proj * mvp.view * model * vec4(inPosition, 1.0);
}  