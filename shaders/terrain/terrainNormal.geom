#version 450
layout (triangles) in;
layout (line_strip, max_vertices = 6) out;

layout(location = 0) in vec3 inNormal[];
layout(location = 1) in vec3 inColor[];
layout(location = 0) out vec3 outColor;
const float MAGNITUDE = 0.1;
  
layout(set = 3, binding = 0) uniform UniformCamera {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

void generateLine(int index) {
    gl_Position = mvp.proj * gl_in[index].gl_Position;
    outColor = inColor[index];
    EmitVertex();
    gl_Position = mvp.proj * (gl_in[index].gl_Position + vec4(inNormal[index], 0.0) * MAGNITUDE);
    outColor = inColor[index];
    EmitVertex();
    EndPrimitive();
}

void main() {	
    generateLine(0); // first vertex normal
    generateLine(1); // second vertex normal
    generateLine(2); // third vertex normal
}