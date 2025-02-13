#version 450
#define epsilon 0.0001 

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 texCoords;
layout(location = 2) in vec4 modelCoords;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout( push_constant ) uniform constants {
    layout(offset = 0) vec3 lightPosition;
    layout(offset = 16) int far;
} PushConstants;

void main() {
    vec4 color = texture(texSampler, texCoords) * vec4(fragColor, 1.0);
    float dx = dFdx(gl_FragCoord.z);
    float dy = dFdy(gl_FragCoord.z);
    outColor = vec4(gl_FragCoord.z, gl_FragCoord.z * gl_FragCoord.z + 0.25 * (dx * dx + dy * dy), 0.0, 1.0);
    if (color.a < epsilon) {
        outColor = vec4(1.0, 1.0, 0.0, 1.0);
    }
}