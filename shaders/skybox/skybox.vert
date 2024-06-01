#version 450

layout(set = 0, binding = 0) uniform UniformCamera {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 TexCoords;

void main() {
    TexCoords = inPosition;
    vec4 position = mvp.proj * mvp.view * mvp.model * vec4(inPosition, 1.0);
    // little trick to force skybox to have z = 1 so it's drawn on the background
    //gl_Position = position.xyww;
    gl_Position = position;
}  