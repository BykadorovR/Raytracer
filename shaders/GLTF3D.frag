#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;
layout(set = 1, binding = 0) uniform sampler2D texSampler;
layout(set = 1, binding = 1) uniform sampler2D normalSampler;

layout(set = 3, binding = 0) uniform AlphaMask {
    bool alphaMask;
    float alphaMaskCutoff;
} alphaMask;

void main() {
    outColor = texture(texSampler, fragTexCoord) * vec4(fragColor, 1.0);
    texture(normalSampler, fragTexCoord);
    if (alphaMask.alphaMask) {
        if (outColor.a < alphaMask.alphaMaskCutoff) {
            discard;
        }
    }
}