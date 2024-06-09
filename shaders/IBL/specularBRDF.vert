#version 450
#define epsilon 0.0001 

layout(set = 0, binding = 0) uniform UniformCamera {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragPosition;

void main() {
    vec4 afterModel = mvp.model * vec4(inPosition, 1.0);
    gl_Position = mvp.proj * mvp.view * afterModel;
    
    fragPosition = inTexCoord;
}