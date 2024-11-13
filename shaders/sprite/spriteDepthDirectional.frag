#version 450
#define epsilon 0.0001 

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 texCoords;
layout(location = 2) in vec4 modelCoords;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout( push_constant ) uniform constants {
    layout(offset = 0) vec3 lightPosition;
    layout(offset = 16) int far;
} PushConstants;

void main() {
    vec4 color = texture(texSampler, texCoords) * vec4(fragColor, 1.0);
    gl_FragDepth = gl_FragCoord.z;
    if (color.a < epsilon) gl_FragDepth = 1.0;
}