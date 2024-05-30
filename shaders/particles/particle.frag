#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outColorBloom;

layout(set = 0, binding = 1) uniform sampler2D particleTexture;

void main() {
    vec4 textureColor = texture(particleTexture, gl_PointCoord);
    outColor = vec4(fragColor.rgb, fragColor.a * textureColor.a);
    // check whether fragment output is higher than threshold, if so output as brightness color
    float brightness = dot(outColor.rgb, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 1.0)
        outColorBloom = vec4(outColor.rgb, 1.0);
    else
        outColorBloom = vec4(0.0, 0.0, 0.0, 1.0);
}