#version 450

layout(set = 0, binding = 0) uniform UniformCamera {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec4 inTangent;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragColor;

void main() {
    // normals should be in the same space as gl_Position, because we will sum position and normals
    mat3 normalMatrix = mat3(transpose(inverse(mvp.view * mvp.model)));
    fragNormal = normalize(normalMatrix * inTangent.xyz);

    fragColor = inColor;
    gl_Position = mvp.view * mvp.model * vec4(inPosition, 1.0);
}  