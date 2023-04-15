#version 450
layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;
layout(location = 3) in vec2 fragTexCoord;

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
    outColor = texture(texSampler, fragTexCoord);
    vec4 stub = texture(normalSampler, fragTexCoord);
    float ambientFactor = lights[push.lightNum - 1].ambient;
    //dot product between normal and light ray
    float diffuseFactor = max(dot(normalize(lights[push.lightNum - 1].lightPosition - fragPosition), normalize(fragNormal)), 0);
    //dot product between reflected ray and light ray
    float specularFactor = lights[push.lightNum - 1].specular * 
                           pow(max(dot(normalize(reflect(fragPosition - lights[push.lightNum - 1].lightPosition, normalize(fragNormal))), normalize(push.cameraPosition - fragPosition)), 0), 32);

    outColor *= vec4((ambientFactor + diffuseFactor) * lights[push.lightNum - 1].lightColor, 1.f);
}