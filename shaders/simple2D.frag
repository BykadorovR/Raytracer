#version 450
layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;
layout(location = 3) in vec2 fragTexCoord;
layout(location = 4) in mat3 fragTBN;

layout(location = 0) out vec4 outColor;
layout(set = 1, binding = 0) uniform sampler2D texSampler;
layout(set = 1, binding = 1) uniform sampler2D normalSampler;

struct Light {
    float ambient;
    float specular;
    vec3 lightColor;
    vec3 lightPosition;
};

layout(std430, set = 2, binding = 0) readonly buffer LightBuffer {
    Light lights[];
};

layout( push_constant ) uniform constants {
    int lightNum;
    vec3 cameraPosition;
} push;

void main() {
    outColor = texture(texSampler, fragTexCoord) * vec4(fragColor, 1.0);
    vec3 normal = texture(normalSampler, fragTexCoord).rgb;
    normal = normal * 2.0 - 1.0;
    normal = normalize(fragTBN * normal);
    vec3 lightFactor = vec3(0.f, 0.f, 0.f);
    for (int i = 0; i < push.lightNum; i++) {
        float ambientFactor = lights[i].ambient;
        //dot product between normal and light ray
        float diffuseFactor = max(dot(normalize(lights[i].lightPosition - fragPosition), normal), 0);
        //dot product between reflected ray and light ray
        float specularFactor = lights[i].specular * 
                               pow(max(dot(normalize(reflect(fragPosition - lights[i].lightPosition, normal)), normalize(push.cameraPosition - fragPosition)), 0), 32);
        lightFactor += (ambientFactor + diffuseFactor + specularFactor) * lights[i].lightColor; 
    }
    outColor *= vec4(lightFactor, 1.f);
}