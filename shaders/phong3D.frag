#version 450

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) in mat3 fragTBN;
//mat3 takes 3 slots
layout(location = 7) in vec4 fragLightDirectionalCoord[2];
layout(location = 9) in vec4 fragLightPointCoord[4];

layout(location = 0) out vec4 outColor;
layout(set = 1, binding = 0) uniform sampler2D texSampler;
layout(set = 1, binding = 1) uniform sampler2D normalSampler;
layout(set = 6, binding = 0) uniform sampler2D shadowDirectionalSampler[2];
layout(set = 6, binding = 1) uniform samplerCube shadowPointSampler[4];

layout(set = 3, binding = 0) uniform AlphaMask {
    bool alphaMask;
    float alphaMaskCutoff;
} alphaMask;

struct LightDirectional {
    float ambient;
    float specular;
    //
    vec3 color;
    vec3 position;
};

struct LightPoint {
    float ambient;
    float specular;
    //attenuation
    float constant;
    float linear;
    float quadratic;
    //
    vec3 color;
    vec3 position;
};

layout(set = 4, binding = 0) readonly buffer LightBufferDirectional {
    int lightDirectionalNumber;
    LightDirectional lightDirectional[];
};

layout(set = 4, binding = 1) readonly buffer LightBufferPoint {
    int lightPointNumber;
    LightPoint lightPoint[];
};

layout( push_constant ) uniform constants {
    vec3 cameraPosition;
} push;

float calculateTextureShadowDirectional(sampler2D shadowSampler, vec4 coords, vec3 normal, vec3 lightDir) {
    // perform perspective divide
    vec3 position = coords.xyz / coords.w;
    // transform to [0,1] range
    position.xy = position.xy * 0.5 + 0.5;
    float currentDepth = position.z;
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    vec2 unitSize = 1.0 / textureSize(shadowSampler, 0);
    float shadow = 0.0;
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            float bufferDepth = texture(shadowSampler, vec2(position.x + x * unitSize.x, 1.0 - (position.y + y * unitSize.y))).r;
            shadow += (currentDepth - bias) > bufferDepth  ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    if(position.z > 1.0)
        shadow = 0.0;
    return shadow;
}

float calculateTextureShadowPoint(samplerCube shadowSampler, vec4 coords, vec3 normal, vec3 lightDir) {
    return 0;
}

vec3 directionalLight(vec3 normal) {
    vec3 lightFactor = vec3(0.f, 0.f, 0.f);
    for (int i = 0; i < lightDirectionalNumber; i++) {
        vec3 lightDir = normalize(lightDirectional[i].position - fragPosition);
        float shadow = calculateTextureShadowDirectional(shadowDirectionalSampler[i], fragLightDirectionalCoord[i], normal, lightDir); 
        float ambientFactor = lightDirectional[i].ambient;
        //dot product between normal and light ray
        float diffuseFactor = max(dot(lightDir, normal), 0);
        //dot product between reflected ray and light ray
        float specularFactor = lightDirectional[i].specular * 
                               pow(max(dot(normalize(reflect(-lightDir, normal)), normalize(push.cameraPosition - fragPosition)), 0), 32);
        lightFactor += (ambientFactor + (1 - shadow) * (diffuseFactor + specularFactor)) * lightDirectional[i].color; 
    }
    return lightFactor;
}

vec3 pointLight(vec3 normal) {
    vec3 lightFactor = vec3(0.f, 0.f, 0.f);
    for (int i = 0; i < lightPointNumber; i++) {
        vec3 lightDir = normalize(lightPoint[i].position - fragPosition);
        float shadow = calculateTextureShadowPoint(shadowPointSampler[i], fragLightPointCoord[i], normal, lightDir); 
        float distance = length(lightPoint[i].position - fragPosition);
        float attenuation = 1.f / (lightPoint[i].constant + lightPoint[i].linear * distance + lightPoint[i].quadratic * distance * distance);
        float ambientFactor = lightPoint[i].ambient;
        //dot product between normal and light ray
        float diffuseFactor = max(dot(lightDir, normal), 0);
        //dot product between reflected ray and light ray
        float specularFactor = lightPoint[i].specular * 
                               pow(max(dot(normalize(reflect(-lightDir, normal)), normalize(push.cameraPosition - fragPosition)), 0), 32);
        lightFactor += (ambientFactor + diffuseFactor + specularFactor) * attenuation * lightPoint[i].color; 
    }

    return lightFactor;
}

void main() {
    outColor = texture(texSampler, fragTexCoord) * vec4(fragColor, 1.0);
    if (alphaMask.alphaMask) {
        if (outColor.a < alphaMask.alphaMaskCutoff) {
            discard;
        }
    }

    if (isnan(fragTBN[0][0]) == false && isnan(fragTBN[1][1]) == false && isnan(fragTBN[2][2]) == false) {
        vec3 normal = texture(normalSampler, fragTexCoord).rgb;
        normal = normal * 2.0 - 1.0;
        normal = normalize(fragTBN * normal);
        vec3 lightFactor = vec3(0.f, 0.f, 0.f);
        //calculate directional light
        lightFactor += directionalLight(normal);
        //calculate point light
        lightFactor += pointLight(normal);
        outColor *= vec4(lightFactor, 1.f);
    }
}