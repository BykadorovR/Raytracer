#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in int inTile;

layout(location = 0) out vec2 TexCoord;
layout(location = 1) flat out int outTile;

void main() {
    gl_Position = vec4(inPosition, 1.0);
    TexCoord = inTexCoord;
    outTile = inTile;
}