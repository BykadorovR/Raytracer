#version 450

layout(location = 0) in float fragHeight;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 tessColor;
layout(location = 3) flat in int outNeighborID[3][3];
layout(location = 12) flat in int outNeighborRotation[3][3];

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outColorBloom;
layout(set = 0, binding = 3) uniform sampler2D texSampler[4];

layout(push_constant) uniform constants {
    layout(offset = 32) float heightLevels[4];
    layout(offset = 48) int patchEdge;
    layout(offset = 64) int tessColorFlag;
    layout(offset = 80) int enableShadow;
    layout(offset = 96) int enableLighting;
    layout(offset = 112) vec3 cameraPosition;
    layout(offset = 128) int pickedPatch;
} push;

mat2 rotate(float a) {
    float s = sin(radians(a));
    float c = cos(radians(a));
    mat2 m = mat2(c, s, -s, c);
    return m;
}

void main() {    
    vec2 texCoord = rotate(outNeighborRotation[1][1]) * fragTexCoord;
    vec2 dx = dFdx(texCoord);
    vec2 dy = dFdy(texCoord);
    int textureID = outNeighborID[1][1];
    if (push.tessColorFlag > 0) {
        outColor = vec4(tessColor, 1);
    } else {
        if (textureID == -1) {
            outColor = textureGrad(texSampler[0], texCoord, dx, dy);
        } else {
            float rate = 0.2;           
            if (fract(fragTexCoord.x) < rate) {
                float weight = smoothstep(-rate, rate, fract(fragTexCoord.x));
                vec4 color1 = textureGrad(texSampler[textureID], texCoord, dx, dy);                
                vec2 texCoord2 = rotate(outNeighborRotation[0][1]) * fragTexCoord;
                vec2 dx2 = dFdx(texCoord2);
                vec2 dy2 = dFdy(texCoord2);
                vec4 color2 = textureGrad(texSampler[outNeighborID[0][1]], texCoord2, dx2, dy2);
                outColor = mix(color2, color1, weight);
            } else if (fract(fragTexCoord.x) > 1 - rate) {
                float weight = smoothstep(1 - rate, 1 + rate, fract(fragTexCoord.x));
                vec4 color1 = textureGrad(texSampler[textureID], texCoord, dx, dy);                
                vec2 texCoord2 = rotate(outNeighborRotation[2][1]) * fragTexCoord;
                vec2 dx2 = dFdx(texCoord2);
                vec2 dy2 = dFdy(texCoord2);
                vec4 color2 = textureGrad(texSampler[outNeighborID[2][1]], texCoord2, dx2, dy2);
                outColor = mix(color1, color2, weight);
            } else {
                outColor = textureGrad(texSampler[textureID], texCoord, dx, dy);
            }
        }
    }

    vec2 line = fract(texCoord);
    if (push.patchEdge > 0 && (line.x < 0.01 || line.y < 0.01 || line.x > 0.99 || line.y > 0.99)) {
        outColor = vec4(1, 1, 0, 1);
        // in fragment shader gl_PrimitiveID behaves the same way as in tesselation shaders IF there is no geometry shader
        if (push.pickedPatch == gl_PrimitiveID) {
            outColor = vec4(1, 0, 0, 1);
        }
    }

    // check whether fragment output is higher than threshold, if so output as brightness color
    float brightness = dot(outColor.rgb, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 1.0)
        outColorBloom = vec4(outColor.rgb, 1.0);
    else
        outColorBloom = vec4(0.0, 0.0, 0.0, 1.0);
}