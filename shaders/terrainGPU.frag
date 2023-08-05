#version 450

layout(location = 0) in float fragHeight;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 tessColor;
layout(location = 4) in vec3 fragPosition;

layout(location = 0) out vec4 outColor;
layout(set = 3, binding = 0) uniform sampler2D terrainSampler[4];

layout( push_constant ) uniform constants {
    layout(offset = 32) float heightLevels[4];
    layout(offset = 48) int patchEdge;
    layout(offset = 64) int tessColorFlag;
    layout(offset = 80) int enableLighting;
    layout(offset = 96) vec3 cameraPosition;
} push;

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
    int distance;
    //parameters
    float far;
    //
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

//It is important to get the gradients before going into non-uniform flow code.
vec2 dx = dFdx(texCoord);
vec2 dy = dFdy(texCoord);
vec4 calculateColor(float max1, float max2, int id1, int id2, float height) {
    vec4 firstTile = textureGrad(terrainSampler[id1], texCoord, dx, dy);
    vec4 secondTile = textureGrad(terrainSampler[id2], texCoord, dx, dy);
    float delta = max2 - max1;
    float factor = (height - max1) / delta;
    return mix(firstTile, secondTile, factor);
}

vec3 directionalLight(vec3 normal) {
    vec3 lightFactor = vec3(0.f, 0.f, 0.f);
    for (int i = 0; i < lightDirectionalNumber; i++) {
        vec3 lightDir = normalize(lightDirectional[i].position - fragPosition);
        float shadow = 0.0;
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
        float distance = length(lightPoint[i].position - fragPosition);
        if (distance > lightPoint[i].distance) break;

        vec3 lightDir = normalize(lightPoint[i].position - fragPosition);
        float shadow = 0.0;
        float attenuation = 1.f / (lightPoint[i].constant + lightPoint[i].linear * distance + lightPoint[i].quadratic * distance * distance);
        float ambientFactor = lightPoint[i].ambient;
        //dot product between normal and light ray
        float diffuseFactor = max(dot(lightDir, normal), 0);
        //dot product between reflected ray and light ray
        float specularFactor = lightPoint[i].specular * 
                               pow(max(dot(normalize(reflect(-lightDir, normal)), normalize(push.cameraPosition - fragPosition)), 0), 32);
        lightFactor += (ambientFactor + (1 - shadow) * (diffuseFactor + specularFactor)) * attenuation * lightPoint[i].color; 
    }

    return lightFactor;
}

void main() {
    if (push.tessColorFlag > 0) {
        outColor = vec4(tessColor, 1);
    } else {
        float height = fragHeight;
        if (height < push.heightLevels[0]) {
            outColor = texture(terrainSampler[0], texCoord);
        } else if (height < push.heightLevels[1]) {
            outColor = calculateColor(push.heightLevels[0], push.heightLevels[1], 0, 1, height);
        } else if (height < push.heightLevels[2]) {
            outColor = calculateColor(push.heightLevels[1], push.heightLevels[2], 1, 2, height);
        } else if (height < push.heightLevels[3]) {
            outColor = calculateColor(push.heightLevels[2], push.heightLevels[3], 2, 3, height);
        } else {
            outColor = texture(terrainSampler[3], texCoord);
        }

        if (push.enableLighting > 0) {
            vec3 lightFactor = vec3(0.f, 0.f, 0.f);
            //calculate directional light
            lightFactor += directionalLight(fragNormal);
            //calculate point light
            lightFactor += pointLight(fragNormal);
            outColor *= vec4(lightFactor, 1.f);
        } 
    }

    vec2 line = fract(texCoord);
    if (push.patchEdge > 0 && (line.x < 0.001 || line.y < 0.001 || line.x > 0.999 || line.y > 0.999)) {
        outColor = vec4(1, 1, 0, 1);
    }
}