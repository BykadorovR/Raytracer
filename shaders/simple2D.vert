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
layout(location = 4) in vec3 inTangent;

layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragColor;
layout(location = 3) out vec2 fragTexCoord;
layout(location = 4) out mat3 fragTBN;

void main() {
    gl_Position = mvp.proj * mvp.view * mvp.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    mat3 normalMatrix = mat3(transpose(inverse(mvp.model)));
    fragNormal = normalMatrix * inNormal;
    vec3 tangent = normalMatrix * inTangent;
    vec3 bitangent = cross(fragNormal, tangent);
    fragTBN = mat3(tangent, bitangent, fragNormal);
    //fragNormal = (mat3(mvp.model) * inNormal).xyz;
    fragPosition = (mvp.model * vec4(inPosition, 1.0)).xyz;
}