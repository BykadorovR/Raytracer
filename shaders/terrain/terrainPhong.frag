#version 450
#define epsilon 0.0001

layout(location = 0) in float fragHeight;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 tessColor;
layout(location = 4) in vec3 fragPosition;
layout(location = 5) in mat3 fragTBN;
//mat3 takes 3 slots
layout(location = 8) in vec4 fragLightDirectionalCoord[2];

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outColorBloom;
layout(set = 0, binding = 3) uniform sampler2D texSampler[4];
layout(set = 0, binding = 4) uniform sampler2D normalSampler[4];
layout(set = 0, binding = 5) uniform sampler2D specularSampler[4];

layout(push_constant) uniform constants {
    layout(offset = 32) float heightLevels[4];
    layout(offset = 48) int patchEdge;
    layout(offset = 64) int tessColorFlag;
    layout(offset = 80) int enableShadow;
    layout(offset = 96) int enableLighting;
    layout(offset = 112) vec3 cameraPosition;
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

//coefficients from base color
layout(set = 0, binding = 6) uniform Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
} material;

//It is important to get the gradients before going into non-uniform flow code.
vec2 dx = dFdx(fragTexCoord);
vec2 dy = dFdy(fragTexCoord);
vec4 calculateColor(float max1, float max2, int id1, int id2, float height) {
    vec4 firstTile = textureGrad(texSampler[id1], fragTexCoord, dx, dy);
    vec4 secondTile = textureGrad(texSampler[id2], fragTexCoord, dx, dy);
    float delta = max2 - max1;
    float factor = (height - max1) / delta;
    return mix(firstTile, secondTile, factor);
}

vec3 calculateNormal(float max1, float max2, int id1, int id2, float height) {
    vec3 firstTile = textureGrad(normalSampler[id1], fragTexCoord, dx, dy).rgb;
    vec3 secondTile = textureGrad(normalSampler[id2], fragTexCoord, dx, dy).rgb;
    float delta = max2 - max1;
    float factor = (height - max1) / delta;
    return mix(firstTile, secondTile, factor);
}

float calculateSpecular(float max1, float max2, int id1, int id2, float height) {
    float firstTile = textureGrad(specularSampler[id1], fragTexCoord, dx, dy).b;
    float secondTile = textureGrad(specularSampler[id2], fragTexCoord, dx, dy).b;
    float delta = max2 - max1;
    float factor = (height - max1) / delta;
    return mix(firstTile, secondTile, factor);
}

#define getLightDir(index) lightDirectional[index]
#define getLightPoint(index) lightPoint[index]
#define getLightAmbient(index) lightAmbient[index]
#define getMaterial() material
#include "../shadow.glsl"
#include "../phong.glsl"

void main() {
    if (push.tessColorFlag > 0) {
        outColor = vec4(tessColor, 1);
    } else {
        float height = fragHeight;
        float specularTexture;
        vec3 normal;
        if (height < push.heightLevels[0]) {
            outColor = texture(texSampler[0], fragTexCoord);
            specularTexture = texture(specularSampler[0], fragTexCoord).b;
            normal = texture(normalSampler[0], fragTexCoord).rgb;
        } else if (height < push.heightLevels[1]) {
            outColor = calculateColor(push.heightLevels[0], push.heightLevels[1], 0, 1, height);
            specularTexture = calculateSpecular(push.heightLevels[0], push.heightLevels[1], 0, 1, height);
            normal = calculateNormal(push.heightLevels[0], push.heightLevels[1], 0, 1, height);
        } else if (height < push.heightLevels[2]) {
            outColor = calculateColor(push.heightLevels[1], push.heightLevels[2], 1, 2, height);
            specularTexture = calculateSpecular(push.heightLevels[1], push.heightLevels[2], 1, 2, height);
            normal = calculateNormal(push.heightLevels[1], push.heightLevels[2], 1, 2, height);
        } else if (height < push.heightLevels[3]) {
            outColor = calculateColor(push.heightLevels[2], push.heightLevels[3], 2, 3, height);
            specularTexture = calculateSpecular(push.heightLevels[2], push.heightLevels[3], 2, 3, height);
            normal = calculateNormal(push.heightLevels[2], push.heightLevels[3], 2, 3, height);
        } else {
            outColor = texture(texSampler[3], fragTexCoord);
            specularTexture = texture(specularSampler[3], fragTexCoord).b;
            normal = texture(normalSampler[3], fragTexCoord).rgb;
        }        

        if (push.enableLighting > 0) {            
            if (length(normal) > epsilon) {
                normal = normal * 2.0 - 1.0;
                normal = normalize(fragTBN * normal);
            } else {
                normal = fragNormal;
            }
            if (length(normal) > epsilon) {
                vec3 lightFactor = vec3(0.0, 0.0, 0.0);
                //calculate directional light
                lightFactor += directionalLight(lightDirectionalNumber, fragPosition, normal, specularTexture, push.cameraPosition, 
                                                push.enableShadow, fragLightDirectionalCoord, shadowDirectionalSampler, 0.005);
                //calculate point light
                lightFactor += pointLight(lightPointNumber, fragPosition, normal, specularTexture,
                                          push.cameraPosition, push.enableShadow, shadowPointSampler, 0.05);
                //calculate ambient light
                for (int i = 0;i < lightAmbientNumber; i++) {
                    lightFactor += lightAmbient[i].color;
                }

                outColor *= vec4(lightFactor, 1.0);
            }
        } 
    }

    vec2 line = fract(fragTexCoord);
    if (push.patchEdge > 0 && (line.x < 0.001 || line.y < 0.001 || line.x > 0.999 || line.y > 0.999)) {
        outColor = vec4(1, 1, 0, 1);
    }

    // check whether fragment output is higher than threshold, if so output as brightness color
    float brightness = dot(outColor.rgb, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 1.0)
        outColorBloom = vec4(outColor.rgb, 1.0);
    else
        outColorBloom = vec4(0.0, 0.0, 0.0, 1.0);
}