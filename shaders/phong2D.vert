#version 450

layout(set = 0, binding = 0) uniform UniformCamera {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

layout(std430, set = 3, binding = 0) readonly buffer LightMatrixDirectional {
    int lightDirectionalNumber;
    mat4 lightDirectionalVP[];
};

layout(std430, set = 3, binding = 1) readonly buffer LightMatrixPoint {
    int lightPointNumber;
    mat4 lightPointVP[];
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec3 inTangent;

layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragColor;
layout(location = 3) out vec2 fragTexCoord;
layout(location = 4) out mat3 fragTBN;
//mat3 takes 3 slots
layout(location = 7) out vec4 fragLightDirectionalCoord[2];
layout(location = 9) out vec4 fragLightPointCoord[4];

void main() {
    vec4 afterModel = mvp.model * vec4(inPosition, 1.0);
    gl_Position = mvp.proj * mvp.view * afterModel;
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    mat3 normalMatrix = mat3(transpose(inverse(mvp.model)));
    fragNormal = normalize(normalMatrix * inNormal);
    vec3 tangent = normalize(normalMatrix * inTangent);
    // re-orthogonalize T with respect to N
    tangent = normalize(tangent - dot(tangent, fragNormal) * fragNormal);
    vec3 bitangent = normalize(cross(fragNormal, tangent));
    
    fragTBN = mat3(tangent, bitangent, fragNormal);
    fragPosition = afterModel.xyz;
    for (int i = 0; i < lightDirectionalNumber; i++)
        fragLightDirectionalCoord[i] = lightDirectionalVP[i] * afterModel;

    for (int i = 0; i < lightPointNumber; i++)
        fragLightPointCoord[i] = lightPointVP[i] * afterModel;
}