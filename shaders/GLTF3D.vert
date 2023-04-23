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
layout(location = 4) in vec4 inJointIndices;
layout(location = 5) in vec4 inJointWeights;
layout(location = 6) in vec4 inTangent;

layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragColor;
layout(location = 3) out vec2 fragTexCoord;
layout(location = 4) out mat3 fragTBN;

layout( push_constant ) uniform constants {
    layout(offset = 32) int jointNum;
} PushConstants;

void main() {
    mat4 skinMat = mat4(1.0);
    if (PushConstants.jointNum > 0) {
        skinMat = inJointWeights.x * jointMatrices[int(inJointIndices.x)] +
                  inJointWeights.y * jointMatrices[int(inJointIndices.y)] +
                  inJointWeights.z * jointMatrices[int(inJointIndices.z)] +
                  inJointWeights.w * jointMatrices[int(inJointIndices.w)];
    }

    mat4 model = mvp.model * skinMat;
    mat3 normalMatrix = mat3(transpose(inverse(model)));

    gl_Position = (mvp.proj * mvp.view * model * vec4(inPosition, 1.0));

    fragColor = inColor;
    fragNormal = normalize(normalMatrix * inNormal);
    vec3 tangent = normalize(normalMatrix * inTangent.xyz);
    tangent = normalize(tangent - dot(tangent, fragNormal) * fragNormal);
    //w stores handness of tbn
    vec3 bitangent = cross(tangent, fragNormal) * inTangent.w;
    fragTBN = mat3(tangent, bitangent, fragNormal);
    fragTexCoord = inTexCoord;
    fragPosition = (model * vec4(inPosition, 1.0)).xyz;
}