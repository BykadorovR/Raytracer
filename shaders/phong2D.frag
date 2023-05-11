#version 450
layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) in mat3 fragTBN;
//mat3 takes 3 slots
layout(location = 7) in vec4 fragShadowCoord;

layout(location = 0) out vec4 outColor;
layout(set = 1, binding = 0) uniform sampler2D texSampler;
layout(set = 1, binding = 1) uniform sampler2D normalSampler;
layout(set = 1, binding = 2) uniform sampler2D shadowSampler;

struct LightDirectional {
    float ambient;
    float specular;
    vec3 color;
    vec3 direction;
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

layout(std430, set = 2, binding = 0) readonly buffer LightBufferDirectional {
    int lightDirectionalNumber;
    LightDirectional lightDirectional[];
};

layout(std430, set = 2, binding = 1) readonly buffer LightBufferPoint {
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
        lightFactor += (ambientFactor + diffuseFactor + specularFactor) * lightDirectional[i].color; 
    }
    return lightFactor;
}

vec3 pointLight(float shadow, vec3 normal) {
    vec3 lightFactor = vec3(0.f, 0.f, 0.f);
    for (int i = 0; i < lightPointNumber; i++) {
        float distance = length(lightPoint[i].position - fragPosition);
        float attenuation = 1.f / (lightPoint[i].constant + lightPoint[i].linear * distance + lightPoint[i].quadratic * distance * distance);
        float ambientFactor = lightPoint[i].ambient;
        //dot product between normal and light ray
        float diffuseFactor = max(dot(normalize(lightPoint[i].position - fragPosition), normal), 0);
        //dot product between reflected ray and light ray
        float specularFactor = lightPoint[i].specular * 
                               pow(max(dot(normalize(reflect(fragPosition - lightPoint[i].position, normal)), normalize(push.cameraPosition - fragPosition)), 0), 32);
        lightFactor += (ambientFactor + (1 - shadow) * (diffuseFactor + specularFactor)) * attenuation * lightPoint[i].color; 
    }

    return lightFactor;
}

float calculateShadow() {
    // perform perspective divide
    vec3 position = fragShadowCoord.xyz / fragShadowCoord.w;
    // transform to [0,1] range
    position.xy = position.xy * 0.5 + 0.5;
    float bufferDepth = texture(shadowSampler, position.xy).r;
    float currentDepth = position.z;
    float bias = 0.005;
    float shadow = currentDepth - bias > bufferDepth  ? 1.0 : 0.0;
    if(position.z > 1.0)
        shadow = 0.0;
    return shadow;
}

void main() {
    float shadow = calculateShadow();

    outColor = texture(texSampler, fragTexCoord) * vec4(fragColor, 1.0);
    vec3 normal = texture(normalSampler, fragTexCoord).rgb;
    normal = normal * 2.0 - 1.0;
    normal = normalize(fragTBN * normal);
    vec3 lightFactor = vec3(0.f, 0.f, 0.f);
    //calculate directional light
    lightFactor += directionalLight(normal);
    //calculate point light
    lightFactor += pointLight(shadow, normal);
    outColor *= vec4(lightFactor, 1.f);
}