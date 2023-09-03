float calculateTextureShadowDirectional(sampler2D shadowSampler, vec4 coords, vec3 normal, vec3 lightDir, float minBias) {
    // perform perspective divide, 
    vec3 position = coords.xyz / coords.w;
    // transform to [0,1] range
    position.xy = position.xy * 0.5 + 0.5;
    float currentDepth = position.z;
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), minBias);
    vec2 unitSize = 1.0 / textureSize(shadowSampler, 0);
    float shadow = 0.0;
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            //IMPORTANT: we flip viewport for depth texture (OpenGL -> Vulkan) so it's correctly displayed in RenderDoc and on screen
            //but we can't just use it here as is, because here is left-hand space and texture is in right-hand space
            //so need to subtract position from 1.0
            float bufferDepth = texture(shadowSampler, vec2(position.x + x * unitSize.x, 1.0 - (position.y + y * unitSize.y))).r;
            shadow += (currentDepth - bias) > bufferDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    if(position.z > 1.0)
        shadow = 0.0;
    return shadow;
}

vec3 sampleOffsetDirections[20] = vec3[]
(
   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);   

float calculateTextureShadowPoint(samplerCube shadowSampler, vec3 fragPosition, vec3 lightPosition, float far, float bias) {
    // perform perspective divide
    vec3 fragToLight = fragPosition - lightPosition;
    float currentDepth = length(fragToLight);
    float shadow = 0.0;
    int samples  = 20;
    float viewDistance = length(push.cameraPosition - fragPosition);
    float diskRadius = (1.0 + (viewDistance / far)) / 25.0;  
    for(int i = 0; i < samples; i++) {
        float closestDepth = texture(shadowSampler, fragToLight + sampleOffsetDirections[i] * diskRadius).r;
        closestDepth *= far;
        if(currentDepth - bias > closestDepth) shadow += 1.0;
    }
    
    shadow /= float(samples); 
    return shadow;
}

vec3 directionalLight(int lightDirectionalNumber, vec3 fragPosition, vec3 normal, vec3 cameraPosition, 
                      int enableShadow, vec4 fragLightDirectionalCoord[2], sampler2D shadowDirectionalSampler[2], float bias) {
    vec3 lightFactor = vec3(0.0, 0.0, 0.0);
    for (int i = 0; i < lightDirectionalNumber; i++) {
        vec3 lightDir = normalize(getLightDir(i).position - fragPosition);
        float shadow = 0.0;
        if (enableShadow > 0)
            shadow = calculateTextureShadowDirectional(shadowDirectionalSampler[i], fragLightDirectionalCoord[i], normal, lightDir, bias); 
        float ambientFactor = getLightDir(i).ambient;
        //dot product between normal and light ray
        float diffuseFactor = max(dot(lightDir, normal), 0);
        //dot product between reflected ray and light ray
        float specularFactor = getLightDir(i).specular * 
                               pow(max(dot(normalize(reflect(-lightDir, normal)), normalize(cameraPosition - fragPosition)), 0), 32);
        lightFactor += (ambientFactor + (1 - shadow) * (diffuseFactor + specularFactor)) * getLightDir(i).color; 
    }
    return lightFactor;
}

vec3 pointLight(int lightPointNumber, vec3 fragPosition, vec3 normal, vec3 cameraPosition, 
                int enableShadow, samplerCube shadowPointSampler[4], float bias) {
    vec3 lightFactor = vec3(0.0, 0.0, 0.0);
    for (int i = 0; i < lightPointNumber; i++) {
        float distance = length(getLightPoint(i).position - fragPosition);
        if (distance > getLightPoint(i).distance) break;

        vec3 lightDir = normalize(getLightPoint(i).position - fragPosition);
        float shadow = 0.0;
        if (enableShadow > 0)
            shadow = calculateTextureShadowPoint(shadowPointSampler[i], fragPosition, getLightPoint(i).position, getLightPoint(i).far, bias); 
        float attenuation = 1.0 / (getLightPoint(i).quadratic * distance * distance);
        float ambientFactor = getLightPoint(i).ambient;
        //dot product between normal and light ray
        float diffuseFactor = max(dot(lightDir, normal), 0);
        //dot product between reflected ray and light ray
        float specularFactor = getLightPoint(i).specular * 
                               pow(max(dot(normalize(reflect(-lightDir, normal)), normalize(cameraPosition - fragPosition)), 0), 32);
        lightFactor += (ambientFactor + (1 - shadow) * (diffuseFactor + specularFactor)) * attenuation * getLightPoint(i).color; 
    }

    return lightFactor;
}