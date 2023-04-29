#version 450

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) in mat3 fragTBN;

layout(location = 0) out vec4 outColor;
layout(set = 1, binding = 0) uniform sampler2D texSampler;
layout(set = 1, binding = 1) uniform sampler2D normalSampler;

layout(set = 3, binding = 0) uniform AlphaMask {
    bool alphaMask;
    float alphaMaskCutoff;
} alphaMask;

struct LightDirectional {
    float ambient;
    float specular;
    vec3 color;
    vec3 direction;
};

struct LightPoint {
    float ambient;
    float specular;
    vec3 color;
    vec3 position;
};

layout(std430, set = 4, binding = 0) readonly buffer LightBufferDirectional {
    int lightDirectionalNumber;
    LightDirectional lightDirectional[];
};

layout(std430, set = 4, binding = 1) readonly buffer LightBufferPoint {
    int lightPointNumber;
    LightPoint lightPoint[];
};

layout( push_constant ) uniform constants {
    vec3 cameraPosition;
} push;

vec3 directionalLight(vec3 normal) {
    vec3 lightFactor = vec3(0.f, 0.f, 0.f);
    for (int i = 0; i < lightDirectionalNumber; i++) {
        float ambientFactor = lightDirectional[i].ambient;
        //dot product between normal and light ray
        float diffuseFactor = max(dot(normalize(-lightDirectional[i].direction), normal), 0);
        //dot product between reflected ray and light ray
        float specularFactor = lightDirectional[i].specular * 
                               pow(max(dot(normalize(reflect(lightDirectional[i].direction, normal)), normalize(push.cameraPosition - fragPosition)), 0), 32);
        lightFactor = (ambientFactor + diffuseFactor + specularFactor) * lightDirectional[i].color; 
    }
    return lightFactor;
}

vec3 pointLight(vec3 normal) {
    vec3 lightFactor = vec3(0.f, 0.f, 0.f);
    for (int i = 0; i < lightPointNumber; i++) {
        float ambientFactor = lightPoint[i].ambient;
        //dot product between normal and light ray
        float diffuseFactor = max(dot(normalize(lightPoint[i].position - fragPosition), normal), 0);
        //dot product between reflected ray and light ray
        float specularFactor = lightPoint[i].specular * 
                               pow(max(dot(normalize(reflect(fragPosition - lightPoint[i].position, normal)), normalize(push.cameraPosition - fragPosition)), 0), 32);
        lightFactor += (ambientFactor + diffuseFactor + specularFactor) * lightPoint[i].color; 
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