#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 tessColor;
layout(location = 3) in vec3 fragPosition;
layout(location = 4) in mat3 fragTBN;
//mat3 takes 3 slots
layout(location = 7) in vec4 fragLightDirectionalCoord[2];
struct PatchDescription {
    int rotation;
    int textureID;
};
// Q00 - Q10 - Q20
// Q01 - Q11 - Q21
// Q02 - Q12 - Q22
// Q11 - current patch
layout(location = 9) flat in PatchDescription inNeighbor[3][3];

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outColorBloom;
layout(set = 0, binding = 4) uniform sampler2D texSampler[4];

layout(push_constant) uniform constants {
    layout(offset = 32) int enableShadow;
    int enableLighting;
    vec3 cameraPosition;
    float stripeLeft;
    float stripeRight;
    float stripeTop;
    float stripeBot;
} push;

struct LightDirectional {
    //
    vec3 color; //radiance
    vec3 position;
};

struct LightPoint {
    //attenuation
    float quadratic;
    int distance;
    //parameters
    float far;
    //
    vec3 color; //radiance
    vec3 position;
};

struct LightAmbient {
    vec3 color; //radiance
};

layout(std140, set = 1, binding = 1) readonly buffer LightBufferDirectional {
    int lightDirectionalNumber;
    LightDirectional lightDirectional[];
};

layout(std140, set = 1, binding = 2) readonly buffer LightBufferPoint {
    int lightPointNumber;
    LightPoint lightPoint[];
};

layout(std140, set = 1, binding = 3) readonly buffer LightBufferAmbient {
    int lightAmbientNumber;
    LightAmbient lightAmbient[];
};

layout(set = 1, binding = 4) uniform sampler2D shadowDirectionalSampler[2];
layout(set = 1, binding = 5) uniform samplerCube shadowPointSampler[4];
layout(set = 1, binding = 6) uniform ShadowParameters {
    int enabledDirectional[2];
    int enabledPoint[4];
    //0 - simple, 1 - vsm
    int algorithmDirectional;
    int algorithmPoint;
} shadowParameters;

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
    vec4 color11 = texture(texSampler[inNeighbor[index11[0]][index11[1]].textureID], texCoord11);

    vec2 texCoord12 = rotate(inNeighbor[index12[0]][index12[1]].rotation) * fragTexCoord;
    vec4 color12 = texture(texSampler[inNeighbor[index12[0]][index12[1]].textureID], texCoord12);

    vec2 texCoord21 = rotate(inNeighbor[index21[0]][index21[1]].rotation) * fragTexCoord;
    vec4 color21 = texture(texSampler[inNeighbor[index21[0]][index21[1]].textureID], texCoord21);

    vec2 texCoord22 = rotate(inNeighbor[index22[0]][index22[1]].rotation) * fragTexCoord;
    vec4 color22 = texture(texSampler[inNeighbor[index22[0]][index22[1]].textureID], texCoord22);

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
    vec4 color1 = texture(texSampler[inNeighbor[index1[0]][index1[1]].textureID], texCoord1);

    vec2 texCoord2 = rotate(inNeighbor[index2[0]][index2[1]].rotation) * fragTexCoord;
    vec4 color2 = texture(texSampler[inNeighbor[index2[0]][index2[1]].textureID], texCoord2);

    float weight1 = (coord - rate1);
    float weight2 = (rate2 - coord);
    return outColor = (weight1 * color1 + weight2 * color2) / (rate2 - rate1);
}

#define getLightDir(index) lightDirectional[index]
#define getLightPoint(index) lightPoint[index]
#define getLightAmbient(index) lightAmbient[index]
#define getMaterial() material
#define getShadowParameters() shadowParameters
#include "../../shadow.glsl"

void main() {
    vec2 texCoord = rotate(inNeighbor[1][1].rotation) * fragTexCoord;
    int textureID = inNeighbor[1][1].textureID;
    
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
        outColor = texture(texSampler[textureID], texCoord);
    }

    vec3 normal = fragNormal;
    vec3 lightFactor = vec3(0.0, 0.0, 0.0);
    for (int i = 0; i < lightDirectionalNumber; i++) {
        float bias = 0.005;
        vec3 lightDir = normalize(getLightDir(i).position - fragPosition);
        float shadow = 0.0;
        if (push.enableShadow > 0 && getShadowParameters().enabledDirectional[i] > 0)
            shadow = calculateTextureShadowDirectional(shadowDirectionalSampler[i], fragLightDirectionalCoord[i], normal, lightDir, bias); 
        lightFactor += getLightDir(i).color * (1 - shadow);
    }
    for (int i = 0; i < lightPointNumber; i++) {
        float bias = 0.05;
        float distance = length(getLightPoint(i).position - fragPosition);
        if (distance > getLightPoint(i).distance) break;
        vec3 lightDir = normalize(getLightPoint(i).position - fragPosition);
        float shadow = 0.0;
        if (push.enableShadow > 0 && getShadowParameters().enabledPoint[i] > 0)
            shadow = calculateTextureShadowPoint(shadowPointSampler[i], fragPosition, getLightPoint(i).position, getLightPoint(i).far, bias); 
        float attenuation = 1.0 / (getLightPoint(i).quadratic * distance * distance);
        lightFactor += getLightPoint(i).color * attenuation * (1 - shadow);
    }

    outColor *= vec4(lightFactor, 1.0);

    // check whether fragment output is higher than threshold, if so output as brightness color
    float brightness = dot(outColor.rgb, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 1.0)
        outColorBloom = vec4(outColor.rgb, 1.0);
    else
        outColorBloom = vec4(0.0, 0.0, 0.0, 1.0);
}