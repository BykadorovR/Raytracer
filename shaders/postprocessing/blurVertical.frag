#version 450

layout(set = 0, binding = 0) uniform sampler2D inputImage;

layout(std430, set = 0, binding = 1) readonly buffer Weights {
    layout(align = 4) float weights[];
};

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 texelSize = vec2(1.0) / vec2(textureSize(inputImage, 0));
    vec4 result = vec4(0.0);
    int radius = int(weights.length() / 2);

    for (int i = -radius; i <= radius; ++i) {
        vec2 offset = vec2(0.0, i) * texelSize;
        result += texture(inputImage, fragTexCoord + offset) * weights[radius + i];
    }

    outColor = result;
}