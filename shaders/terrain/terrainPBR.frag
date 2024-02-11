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
layout(set = 4, binding = 0) uniform sampler2D texSampler[4];
layout(set = 4, binding = 1) uniform sampler2D normalSampler[4];
layout(set = 4, binding = 2) uniform sampler2D metallicSampler[4];
layout(set = 4, binding = 3) uniform sampler2D roughnessSampler[4];
layout(set = 4, binding = 4) uniform sampler2D occlusionSampler[4];
layout(set = 4, binding = 5) uniform sampler2D emissiveSampler[4];
layout(set = 4, binding = 6) uniform samplerCube irradianceSampler;
layout(set = 4, binding = 7) uniform samplerCube specularIBLSampler;
layout(set = 4, binding = 8) uniform sampler2D specularBRDFSampler;
layout(set = 5, binding = 0) uniform sampler2D shadowDirectionalSampler[2];
layout(set = 5, binding = 1) uniform samplerCube shadowPointSampler[4];

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

layout(std140, set = 6, binding = 0) readonly buffer LightBufferDirectional {
    LightDirectional lightDirectional[];
};

layout(std140, set = 6, binding = 1) readonly buffer LightBufferPoint {
    LightPoint lightPoint[];
};

layout(set = 7, binding = 0) uniform Material {
    float metallicFactor;
    float roughnessFactor;
    // occludedColor = mix(color, color * <sampled occlusion texture value>, <occlusion strength>)
    float occlusionStrength;
    vec3 emissiveFactor;
} material;

layout(set = 8, binding = 0) uniform AlphaMask {
    bool alphaMask;
    float alphaMaskCutoff;
} alphaMask;

layout(push_constant) uniform constants {
    layout(offset = 32) float heightLevels[4];
    layout(offset = 48) int patchEdge;
    layout(offset = 64) int tessColorFlag;
    layout(offset = 80) int enableShadow;
    layout(offset = 96) int enableLighting;
    layout(offset = 112) vec3 cameraPosition;
} push;

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

float calculateMetallic(float max1, float max2, int id1, int id2, float height) {
    float firstTile = textureGrad(metallicSampler[id1], fragTexCoord, dx, dy).b;
    float secondTile = textureGrad(metallicSampler[id2], fragTexCoord, dx, dy).b;
    float delta = max2 - max1;
    float factor = (height - max1) / delta;
    return mix(firstTile, secondTile, factor);
}

float calculateRoughness(float max1, float max2, int id1, int id2, float height) {
    float firstTile = textureGrad(roughnessSampler[id1], fragTexCoord, dx, dy).g;
    float secondTile = textureGrad(roughnessSampler[id2], fragTexCoord, dx, dy).g;
    float delta = max2 - max1;
    float factor = (height - max1) / delta;
    return mix(firstTile, secondTile, factor);
}

float calculateOcclusion(float max1, float max2, int id1, int id2, float height) {
    float firstTile = textureGrad(occlusionSampler[id1], fragTexCoord, dx, dy).r;
    float secondTile = textureGrad(occlusionSampler[id2], fragTexCoord, dx, dy).r;
    float delta = max2 - max1;
    float factor = (height - max1) / delta;
    return mix(firstTile, secondTile, factor);
}

vec3 calculateEmissive(float max1, float max2, int id1, int id2, float height) {
    vec3 firstTile = textureGrad(emissiveSampler[id1], fragTexCoord, dx, dy).rgb;
    vec3 secondTile = textureGrad(emissiveSampler[id2], fragTexCoord, dx, dy).rgb;
    float delta = max2 - max1;
    float factor = (height - max1) / delta;
    return mix(firstTile, secondTile, factor);
}

#define getLightDir(index) lightDirectional[index]
#define getLightPoint(index) lightPoint[index]
#include "../pbr.glsl"

void main() {
    if (push.tessColorFlag > 0) {
        outColor = vec4(tessColor, 1);
    } else {
        float height = fragHeight;
        vec4 albedoTexture;
        vec3 normalTexture;
        float metallicTexture;
        float roughnessTexture;
        float occlusionTexture;
        vec3 emissiveTexture;
        if (height < push.heightLevels[0]) {
            albedoTexture = texture(texSampler[0], fragTexCoord);
            normalTexture = texture(normalSampler[0], fragTexCoord).rgb;
            metallicTexture = texture(metallicSampler[0], fragTexCoord).b;
            roughnessTexture = texture(roughnessSampler[0], fragTexCoord).g;
            occlusionTexture = texture(occlusionSampler[0], fragTexCoord).r;
            emissiveTexture = texture(emissiveSampler[0], fragTexCoord).rgb;
        } else if (height < push.heightLevels[1]) {
            albedoTexture = calculateColor(push.heightLevels[0], push.heightLevels[1], 0, 1, height);
            normalTexture = calculateNormal(push.heightLevels[0], push.heightLevels[1], 0, 1, height);
            metallicTexture = calculateMetallic(push.heightLevels[0], push.heightLevels[1], 0, 1, height);
            roughnessTexture = calculateRoughness(push.heightLevels[0], push.heightLevels[1], 0, 1, height);
            occlusionTexture = calculateOcclusion(push.heightLevels[0], push.heightLevels[1], 0, 1, height);
            emissiveTexture = calculateEmissive(push.heightLevels[0], push.heightLevels[1], 0, 1, height);
        } else if (height < push.heightLevels[2]) {
            albedoTexture = calculateColor(push.heightLevels[1], push.heightLevels[2], 1, 2, height);
            normalTexture = calculateNormal(push.heightLevels[1], push.heightLevels[2], 1, 2, height);
            metallicTexture = calculateMetallic(push.heightLevels[1], push.heightLevels[2], 1, 2, height);
            roughnessTexture = calculateRoughness(push.heightLevels[1], push.heightLevels[2], 1, 2, height);
            occlusionTexture = calculateOcclusion(push.heightLevels[1], push.heightLevels[2], 1, 2, height);
            emissiveTexture = calculateEmissive(push.heightLevels[1], push.heightLevels[2], 1, 2, height);
        } else if (height < push.heightLevels[3]) {
            albedoTexture = calculateColor(push.heightLevels[2], push.heightLevels[3], 2, 3, height);
            normalTexture = calculateNormal(push.heightLevels[2], push.heightLevels[3], 2, 3, height);
            metallicTexture = calculateMetallic(push.heightLevels[2], push.heightLevels[3], 2, 3, height);
            roughnessTexture = calculateRoughness(push.heightLevels[2], push.heightLevels[3], 2, 3, height);
            occlusionTexture = calculateOcclusion(push.heightLevels[2], push.heightLevels[3], 2, 3, height);
            emissiveTexture = calculateEmissive(push.heightLevels[2], push.heightLevels[3], 2, 3, height);
        } else {
            albedoTexture = texture(texSampler[3], fragTexCoord);
            normalTexture = texture(normalSampler[3], fragTexCoord).rgb;
            metallicTexture = texture(metallicSampler[3], fragTexCoord).b;
            roughnessTexture = texture(roughnessSampler[3], fragTexCoord).g;
            occlusionTexture = texture(occlusionSampler[3], fragTexCoord).r;
            emissiveTexture = texture(emissiveSampler[3], fragTexCoord).rgb;
        }
        outColor = albedoTexture;

        float metallicValue = metallicTexture * material.metallicFactor;
        float roughnessValue = roughnessTexture * material.roughnessFactor;

        if (alphaMask.alphaMask) {
            if (outColor.a < alphaMask.alphaMaskCutoff) {
                discard;
            }
        }

        if (push.enableLighting > 0) {
            vec3 normal = normalTexture;
            if (length(normal) > epsilon) {
                normal = normal * 2.0 - 1.0;
                normal = normalize(fragTBN * normal);
            } else {
                normal = fragNormal;
            }

            if (length(normal) > epsilon) {
                //calculate reflected part for every light source separately and them sum them            
                vec3 viewDir = normalize(push.cameraPosition - fragPosition);

                // reflectance equation
                vec3 Lr = vec3(0.0);
                for (int i = 0; i < lightDirectional.length(); i++) {
                    vec3 lightDir = normalize(getLightDir(i).position - fragPosition);
                    vec3 inRadiance = getLightDir(i).color;
                    Lr += calculateOutRadiance(lightDir, normal, viewDir, inRadiance, metallicValue, roughnessValue, albedoTexture.rgb);
                }

                for (int i = 0; i < lightPoint.length(); i++) {
                    vec3 lightDir = normalize(getLightPoint(i).position - fragPosition);
                    float distance = length(getLightPoint(i).position - fragPosition);
                    if (distance > getLightPoint(i).distance) break;
                    float attenuation = 1.0 / (getLightPoint(i).quadratic * distance * distance);
                    vec3 inRadiance = getLightPoint(i).color * attenuation;
                    Lr += calculateOutRadiance(lightDir, normal, viewDir, inRadiance, metallicValue, roughnessValue, albedoTexture.rgb);
                }

                outColor.rgb = Lr;
                //add occlusion to resulting color (it doesn't depend on light sources at all), occlusion is stored inside metallic roughness as .r channel or as separate texture .r channel
                //so it doesn't matter, any texture -> .r channel

                //IBL
                vec3 F0 = vec3(0.04);
                F0 = mix(F0, albedoTexture.rgb, metallicValue);
                vec3 kS = fresnelSchlickRoughness(max(dot(normal, viewDir), 0.0), F0, roughnessValue);
                vec3 kD = 1.0 - kS;
                kD *= 1.0 - metallicValue;
                vec3 irradiance = texture(irradianceSampler, normal).rgb;
                vec3 diffuse = kD * irradiance * albedoTexture.rgb;

                vec3 R = reflect(-viewDir, normal);   
                const float MAX_REFLECTION_LOD = 4.0;
                vec3 prefilteredColor = textureLod(specularIBLSampler, R,  roughnessValue * MAX_REFLECTION_LOD).rgb;
                vec2 envBRDF  = texture(specularBRDFSampler, vec2(max(dot(normal, viewDir), 0.0), roughnessValue)).rg;
                vec3 specular = prefilteredColor * (kS * envBRDF.x + envBRDF.y);

                vec3 ambientColor = mix(diffuse + specular, (diffuse + specular) * occlusionTexture, material.occlusionStrength);
                outColor.rgb += ambientColor;

                //add emissive to resulting reflected radiance from all light sources
                outColor.rgb += emissiveTexture * material.emissiveFactor;
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