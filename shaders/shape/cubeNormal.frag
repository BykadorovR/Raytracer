#version 450

layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outColorBloom;
void main() {
    outColor = vec4(inColor, 1.0);
    // check whether fragment output is higher than threshold, if so output as brightness color
    float brightness = dot(outColor.rgb, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 1.0)
        outColorBloom = vec4(outColor.rgb, 1.0);
    else
        outColorBloom = vec4(0.0, 0.0, 0.0, 1.0);
}  