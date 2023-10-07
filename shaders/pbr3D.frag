#version 450
#define epsilon 0.0001

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) in mat3 fragTBN;
//mat3 takes 3 slots
layout(location = 7) in vec4 fragLightDirectionalCoord[2];

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outColorBloom;
layout(set = 2, binding = 0) uniform sampler2D texSampler;
layout(set = 2, binding = 1) uniform sampler2D normalSampler;
layout(set = 2, binding = 2) uniform sampler2D metallicRoughnessSampler;
layout(set = 2, binding = 3) uniform sampler2D occlusionSampler;
layout(set = 2, binding = 4) uniform sampler2D emissiveSampler;

layout(set = 6, binding = 0) uniform sampler2D shadowDirectionalSampler[2];
layout(set = 6, binding = 1) uniform samplerCube shadowPointSampler[4];

layout(set = 3, binding = 0) uniform AlphaMask {
    bool alphaMask;
    float alphaMaskCutoff;
} alphaMask;

struct LightDirectional {
    vec3 ambient;
    //it's not "native" for light source to vary specular
    //it's here for simplification of changing light propery for bulk of objects
    vec3 diffuse;
    vec3 specular;
    //
    vec3 color;
    vec3 position;
};

struct LightPoint {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    //attenuation
    float quadratic;
    int distance;
    //parameters
    float far;
    //
    vec3 color;
    vec3 position;
};

layout(std140, set = 4, binding = 0) readonly buffer LightBufferDirectional {
    LightDirectional lightDirectional[];
};

layout(std140, set = 4, binding = 1) readonly buffer LightBufferPoint {
    LightPoint lightPoint[];
};

//coefficients from base color
layout(set = 7, binding = 0) uniform Material {
    float metallicFactor;
    float roughnessFactor;
    // occludedColor = mix(color, color * <sampled occlusion texture value>, <occlusion strength>)
    float occlusionStrength;
    vec3 emissiveFactor;
} material;

layout( push_constant ) uniform constants {
    layout(offset = 0) int enableShadow;
    layout(offset = 16) int enableLighting;
    layout(offset = 32) vec3 cameraPosition;
} push;

#define getLightDir(index) lightDirectional[index]
#define getLightPoint(index) lightPoint[index]
#include "pbr.glsl"

void main() {
    vec4 albedoTexture = texture(texSampler, fragTexCoord) * vec4(fragColor, 1.0);
    vec4 normalTexture = texture(normalSampler, fragTexCoord);
    vec4 metallicRoughnessTexture = texture(metallicRoughnessSampler, fragTexCoord);
    vec4 occlusionTexture = texture(occlusionSampler, fragTexCoord);
    vec4 emissiveTexture = texture(emissiveSampler, fragTexCoord);
    // ao .r, roughness .g, metallic .b
    float metallicValue = metallicRoughnessTexture.b * material.metallicFactor;
    float roughnessValue = metallicRoughnessTexture.g * material.roughnessFactor;

    outColor = albedoTexture;
    if (alphaMask.alphaMask) {
        if (outColor.a < alphaMask.alphaMaskCutoff) {
            discard;
        }
    }

    if (push.enableLighting > 0) {
        vec3 normal = normalTexture.rgb;
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
            outColor.rgb = mix(outColor.rgb, outColor.rgb * occlusionTexture.r, material.occlusionStrength);
            //add emissive to resulting reflected radiance from all light sources
            outColor.rgb += emissiveTexture.rgb * material.emissiveFactor;

            //outColor.rgb = normal;

        }
    }

    // check whether fragment output is higher than threshold, if so output as brightness color
    float brightness = dot(outColor.rgb, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 1.0)
        outColorBloom = vec4(outColor.rgb, 1.0);
    else
        outColorBloom = vec4(0.0, 0.0, 0.0, 1.0);
}