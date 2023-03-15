#version 450

#define PI 3.1415926538

layout(binding = 0) uniform UniformCamera {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 pos;
} cam;

struct PointLight {
    vec3 pos;
    vec3 color;
    float radius;
};

layout(binding = 2) uniform PointLights {
    int number;
    PointLight light[16];
} pLights;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;
layout(location = 3) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;
layout(binding = 1) uniform sampler2D texSampler;

float calculateNDF(vec3 halfwayDir, float cosNH, float roughness)
{
    float r_sqr = roughness*roughness;
    float cosNH_sqr = cosNH*cosNH;

    return r_sqr / (PI * pow(cosNH_sqr*(r_sqr - 1) + 1,2));
}

float Gschlick(float cosNV, float k)
{
    return cosNV/(cosNV*(1.0-k)+k);
}

float calculateG(vec3 lightDir, vec3 viewDir, float roughness)
{
    float k = (roughness+1.0)*(roughness+1.0)/8.0;
    float cosNV = max(dot(fragNormal,viewDir), 0.0);
    float cosNL = max(dot(fragNormal,lightDir), 0.0);
    return Gschlick(cosNV, k) * Gschlick(cosNL, k);
}

vec3 calculateF(float cosNV, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - cosNV,5);
}

void main() {
    vec3 texColor = texture(texSampler, fragTexCoord).xyz;
    float distanceMultiplier = pow(length(fragPos-cam.pos),3)+1;
    vec3 f;
    vec3 viewDir = normalize(cam.pos - fragPos);
    float roughness = pow(1.0-(texColor.x+texColor.y+texColor.z)/3.0, 4.0);
    float metalness = pow(1.0-(texColor.x+texColor.y+texColor.z)/3.0, 32.0);

    vec3 lightPortion = vec3(0.0,0.0,0.0);
    for (int i = 0; i < pLights.number; i++)
    {
        float attenuation = 1.0 - min(1,pow(length(fragPos-pLights.light[i].pos)/pLights.light[i].radius,2));

        vec3 lightDir = normalize(pLights.light[i].pos - fragPos);
        vec3 halfwayDir = normalize(lightDir + viewDir);

        float cosNH = max(dot(fragNormal, halfwayDir), 0.0);
        float cosNL = max(dot(fragNormal, lightDir), 0.0);
        float cosNV = max(dot(fragNormal, viewDir), 0.0);
        float ndf = calculateNDF(halfwayDir, cosNH, roughness);
        float g = calculateG(lightDir, viewDir, roughness);
        vec3 f0 = mix(vec3(0.04), texColor, metalness); 
        vec3 f = calculateF(cosNV, f0);

        vec3 BRDF = ndf*f*g/( 4.0 * cosNV * cosNL  + 0.0001); // ndf*f*g/( 4.0 * cosNV * cosNL  + 0.0001);

        float Kspec = (f.x+f.y+f.z)/3;
        vec3 Kdiff = (1.0 - f)*(1 - metalness);
        lightPortion = lightPortion + pLights.light[i].color*(Kdiff*texColor/PI + BRDF)*cosNL*attenuation;
    }
    vec3 ambient = vec3(0.03) * texColor;
    vec3 color = lightPortion + ambient;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/1.1)); 
    outColor = vec4(color, 1.0); //vec4(texColor*lightPortion/distanceMultiplier, 1.);
}