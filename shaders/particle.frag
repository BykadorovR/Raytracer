#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outColorBloom;

layout(set = 1, binding = 0) uniform sampler2D particleTexture;

void main() {
    vec4 textureColor = texture(particleTexture, gl_PointCoord);
    outColor = vec4(fragColor.rgb, fragColor.a * textureColor.a);
    outColorBloom = outColor;
}