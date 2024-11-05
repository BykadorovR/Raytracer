#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 tessColor;
layout(location = 2) in vec4 inTilesWeights;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outColorBloom;
layout(set = 0, binding = 4) uniform sampler2D texSampler[4];

layout(push_constant) uniform constants {
    layout(offset = 32) float heightLevels[4];
    int patchEdge;
    int tessColorFlag;
    int enableShadow;
    int enableLighting;
    int pickedPatch;
    float stripeLeft;
    float stripeRight;
    float stripeTop;
    float stripeBot;
} push;

mat2 rotate(float a) {
    float s = sin(radians(a));
    float c = cos(radians(a));
    mat2 m = mat2(c, s, -s, c);
    return m;
}

void main() {
    vec2 texCoord = fragTexCoord;
    vec2 dx = dFdx(texCoord);
    vec2 dy = dFdy(texCoord);
    if (push.tessColorFlag > 0) {
        outColor = vec4(tessColor, 1);
    } else {
        //r = 0, g = 1, b = 2, a = 3
        vec3 texture0 = textureGrad(texSampler[0], texCoord, dx, dy).rgb;
        vec3 texture1 = textureGrad(texSampler[1], texCoord, dx, dy).rgb;
        vec3 texture2 = textureGrad(texSampler[2], texCoord, dx, dy).rgb;
        vec3 texture3 = textureGrad(texSampler[3], texCoord, dx, dy).rgb;
        outColor = vec4(inTilesWeights[0] * texture0 + 
                        inTilesWeights[1] * texture1 +
                        inTilesWeights[2] * texture2 + 
                        inTilesWeights[3] * texture3, 1);
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