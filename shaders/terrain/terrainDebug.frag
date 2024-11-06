#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 tessColor;
struct PatchDescription {
    int rotation;
    int textureID;
};
// Q00 - Q10 - Q20
// Q01 - Q11 - Q21
// Q02 - Q12 - Q22
// Q11 - current patch
layout(location = 2) flat in PatchDescription inNeighbor[3][3];

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

// Q12 -- Q22
//  |      |
//  |      |
// Q11 -- Q21
vec4 getColorCorner(ivec2 index11, ivec2 index12, ivec2 index21, ivec2 index22, float rateLeft, float rateRight, float rateTop, float rateBot) {
    vec2 texCoord11 = rotate(inNeighbor[index11[0]][index11[1]].rotation) * fragTexCoord;
    vec2 dx11 = dFdx(texCoord11);
    vec2 dy11 = dFdy(texCoord11);
    vec4 color11 = textureGrad(texSampler[inNeighbor[index11[0]][index11[1]].textureID], texCoord11, dx11, dy11);

    vec2 texCoord12 = rotate(inNeighbor[index12[0]][index12[1]].rotation) * fragTexCoord;
    vec2 dx12 = dFdx(texCoord12);
    vec2 dy12 = dFdy(texCoord12);
    vec4 color12 = textureGrad(texSampler[inNeighbor[index12[0]][index12[1]].textureID], texCoord12, dx12, dy12);

    vec2 texCoord21 = rotate(inNeighbor[index21[0]][index21[1]].rotation) * fragTexCoord;
    vec2 dx21 = dFdx(texCoord21);
    vec2 dy21 = dFdy(texCoord21);
    vec4 color21 = textureGrad(texSampler[inNeighbor[index21[0]][index21[1]].textureID], texCoord21, dx21, dy21);

    vec2 texCoord22 = rotate(inNeighbor[index22[0]][index22[1]].rotation) * fragTexCoord;
    vec2 dx22 = dFdx(texCoord22);
    vec2 dy22 = dFdy(texCoord22);
    vec4 color22 = textureGrad(texSampler[inNeighbor[index22[0]][index22[1]].textureID], texCoord22, dx22, dy22);

    float den = (rateRight - rateLeft) * (rateTop - rateBot);
    float weight11 = (rateRight - fract(fragTexCoord.x)) * (rateTop - fract(fragTexCoord.y));
    float weight12 = (rateRight - fract(fragTexCoord.x)) * (fract(fragTexCoord.y) - rateBot);
    float weight21 = (fract(fragTexCoord.x) - rateLeft) * (rateTop - fract(fragTexCoord.y));
    float weight22 = (fract(fragTexCoord.x) - rateLeft) * (fract(fragTexCoord.y) - rateBot);
    return (weight11 * color11 + weight12 * color12 + weight21 * color21 + weight22 * color22) / den;
}

//for X - first right Q, then left Q
//for Y - first bot Q, then top Q
vec4 getColorSide(ivec2 index1, ivec2 index2, float coord, float rate1, float rate2) {
    vec2 texCoord1 = rotate(inNeighbor[index1[0]][index1[1]].rotation) * fragTexCoord;
    vec2 dx1 = dFdx(texCoord1);
    vec2 dy1 = dFdy(texCoord1);
    vec4 color1 = textureGrad(texSampler[inNeighbor[index1[0]][index1[1]].textureID], texCoord1, dx1, dy1);

    vec2 texCoord2 = rotate(inNeighbor[index2[0]][index2[1]].rotation) * fragTexCoord;
    vec2 dx2 = dFdx(texCoord2);
    vec2 dy2 = dFdy(texCoord2);
    vec4 color2 = textureGrad(texSampler[inNeighbor[index2[0]][index2[1]].textureID], texCoord2, dx2, dy2);

    float weight1 = (coord - rate1);
    float weight2 = (rate2 - coord);
    return (weight1 * color1 + weight2 * color2) / (rate2 - rate1);
}

void main() {
    vec2 texCoord = rotate(inNeighbor[1][1].rotation) * fragTexCoord;
    vec2 dx = dFdx(texCoord);
    vec2 dy = dFdy(texCoord);
    int textureID = inNeighbor[1][1].textureID;
    if (push.tessColorFlag > 0) {
        outColor = vec4(tessColor, 1);
    } else {
        if (fract(fragTexCoord.x) < push.stripeLeft && fract(fragTexCoord.y) < push.stripeTop) {
            // Q00 --- Q10
            //  |   |   |
            //    - | - | - -  
            //  |   |   |
            // Q01 --- Q11
            //      |
            //      |
            outColor = getColorCorner(ivec2(0, 1), ivec2(0, 0), ivec2(1, 1), ivec2(1, 0), -push.stripeRight, push.stripeLeft, -push.stripeBot, push.stripeTop);
        } else if (fract(fragTexCoord.x) > 1 - push.stripeRight && fract(fragTexCoord.y) > 1 - push.stripeBot) {
            //        |
            //        |
            //   Q11 --- Q21
            //    |   |   |
            //- -   - | - |
            //    |   |   |
            //   Q12 --- Q22
            outColor = getColorCorner(ivec2(1, 2), ivec2(1, 1), ivec2(2, 2), ivec2(2, 1), 1 - push.stripeRight, 1 + push.stripeLeft, 1 - push.stripeBot, 1 + push.stripeTop);
        } else if (fract(fragTexCoord.x) > 1 - push.stripeRight && fract(fragTexCoord.y) < push.stripeTop) {
            //   Q10 --- Q20
            //    |   |   |
            //- -   - | - |
            //    |   |   |
            //   Q11 --- Q21
            //        |
            //        |
            outColor = getColorCorner(ivec2(1, 1), ivec2(1, 0), ivec2(2, 1), ivec2(2, 0), 1 - push.stripeRight, 1 + push.stripeLeft, -push.stripeBot, push.stripeTop);            
        } else if (fract(fragTexCoord.x) < push.stripeLeft && fract(fragTexCoord.y) > 1 - push.stripeBot) {
            //      |
            //      |
            // Q01 --- Q11
            //  |   |   |
            //    - | - | - -
            //  |   |   |
            // Q02 --- Q12
            outColor = getColorCorner(ivec2(0, 2), ivec2(0, 1), ivec2(1, 2), ivec2(1, 1), -push.stripeRight, push.stripeLeft, 1 - push.stripeBot, 1 + push.stripeTop);
        } else if (fract(fragTexCoord.x) < push.stripeLeft && fract(fragTexCoord.y) > push.stripeTop && fract(fragTexCoord.y) < 1 - push.stripeBot) {
            //      _  _
            //     |
            // Q01 - Q11 -
            //     |
            outColor = getColorSide(ivec2(1, 1), ivec2(0, 1), fract(fragTexCoord.x), -push.stripeRight, push.stripeLeft);
        } else if (fract(fragTexCoord.x) > 1 - push.stripeRight && fract(fragTexCoord.y) > push.stripeTop && fract(fragTexCoord.y) < 1 - push.stripeBot) {
            //   _  _
            //       |
            // - Q11 - Q21
            //       |
            outColor = getColorSide(ivec2(2, 1), ivec2(1, 1), fract(fragTexCoord.x), 1 - push.stripeRight, 1 + push.stripeLeft);
        } else if (fract(fragTexCoord.y) < push.stripeTop && fract(fragTexCoord.x) > push.stripeLeft && fract(fragTexCoord.x) < 1 - push.stripeRight) {
            //   Q10
            //  - | -
            //   Q11
            //    |
            outColor = getColorSide(ivec2(1, 1), ivec2(1, 0), fract(fragTexCoord.y), -push.stripeBot, push.stripeTop);
        } else if (fract(fragTexCoord.y) > 1 - push.stripeBot && fract(fragTexCoord.x) > push.stripeLeft && fract(fragTexCoord.x) < 1 - push.stripeRight) {
            //    |
            //   Q11
            //  - | -
            //   Q12 
            outColor = getColorSide(ivec2(1, 2), ivec2(1, 1), fract(fragTexCoord.y), 1 - push.stripeBot, 1 + push.stripeTop);
        } else {
            outColor = textureGrad(texSampler[textureID], texCoord, dx, dy);
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