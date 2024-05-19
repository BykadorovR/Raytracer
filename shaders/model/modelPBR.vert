#version 450
#define epsilon 0.0001 

layout(set = 0, binding = 0) uniform UniformCamera {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

layout(std140, set = 1, binding = 0) readonly buffer JointMatrices {
    mat4 jointMatrices[];
};

layout(std140, set = 3, binding = 0) readonly buffer LightMatrixDirectional {
    int lightDirectionalNumber;
    mat4 lightDirectionalVP[];
};

layout(std140, set = 3, binding = 1) readonly buffer LightMatrixPoint {
    int lightPointNumber;
    mat4 lightPointVP[];
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
//mat3 takes 3 slots
layout(location = 7) out vec4 fragLightDirectionalCoord[2];

void main() {
    mat4 skinMat = mat4(1.0);
    //we pass all 
    skinMat = inJointWeights.x * jointMatrices[int(inJointIndices.x)] +
              inJointWeights.y * jointMatrices[int(inJointIndices.y)] +
              inJointWeights.z * jointMatrices[int(inJointIndices.z)] +
              inJointWeights.w * jointMatrices[int(inJointIndices.w)];

    mat4 model = mvp.model * skinMat;
    mat3 normalMatrix = mat3(transpose(inverse(model)));

    vec4 afterModel = model * vec4(inPosition, 1.0);
    gl_Position = (mvp.proj * mvp.view * afterModel);

    fragColor = inColor;
    fragTexCoord = inTexCoord;
    fragPosition = afterModel.xyz;
    
    fragNormal = normalize(normalMatrix * inNormal);
    fragTBN = mat3(1.0);
    if (length(inTangent) > epsilon) {
        vec3 tangent = normalize(normalMatrix * inTangent.xyz);
        tangent = normalize(tangent - dot(tangent, fragNormal) * fragNormal);
        //w stores handness of tbn
        vec3 bitangent = normalize(cross(tangent, fragNormal)) * inTangent.w;
        fragTBN = mat3(tangent, bitangent, fragNormal);
    }
    for (int i = 0; i < lightDirectionalNumber; i++)
        fragLightDirectionalCoord[i] = lightDirectionalVP[i] * afterModel;
}