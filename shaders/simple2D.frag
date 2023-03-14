#version 450

struct PointLight {
    vec3 pos;
    vec3 color;
    float radius;
};

layout(binding = 2) uniform PointLights {
    int number;
    PointLight light[16];
} pLights;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragPos;

layout(location = 0) out vec4 outColor;
layout(binding = 1) uniform sampler2D texSampler;

void main() {
    vec3 texColor = texture(texSampler, fragTexCoord).xyz;
    float distanceMultiplier = pow(length(fragPos),3)+1;
    vec3 lightPortion = vec3(0,0,0);
    for (int i = 0; i < pLights.number; i++)
    {
        float lightDistRatio = min(1,pow(length(fragPos-pLights.light[i].pos)/pLights.light[i].radius,2));
        lightPortion = lightPortion + pLights.light[i].color*(1-lightDistRatio);
    }
    outColor = vec4(texColor*lightPortion, 1.); //texture(texSampler, fragTexCoord);
}