vec2 poissonDisk[16] = vec2[]( 
   vec2( -0.94201624, -0.39906216 ), 
   vec2( 0.94558609, -0.76890725 ), 
   vec2( -0.094184101, -0.92938870 ), 
   vec2( 0.34495938, 0.29387760 ), 
   vec2( -0.91588581, 0.45771432 ), 
   vec2( -0.81544232, -0.87912464 ), 
   vec2( -0.38277543, 0.27676845 ), 
   vec2( 0.97484398, 0.75648379 ), 
   vec2( 0.44323325, -0.97511554 ), 
   vec2( 0.53742981, -0.47373420 ), 
   vec2( -0.26496911, -0.41893023 ), 
   vec2( 0.79197514, 0.19090188 ), 
   vec2( -0.24188840, 0.99706507 ), 
   vec2( -0.81409955, 0.91437590 ), 
   vec2( 0.19984126, 0.78641367 ), 
   vec2( 0.14383161, -0.14100790 ) 
);

// Returns a random number based on a vec3 and an int.
float random(vec3 seed, int i){
    vec4 seed4 = vec4(seed,i);
    float dot_product = dot(seed4, vec4(12.9898,78.233,45.164,94.673));
    return fract(sin(dot_product) * 43758.5453);
}

float calculateTextureShadowDirectionalPoisson(sampler2D shadowSampler, vec4 coords, vec3 normal, vec3 lightDir, float minBias) {
    // perform perspective divide, 
    vec3 position = coords.xyz / coords.w;
    // transform to [0,1] range
    position.xy = position.xy * 0.5 + 0.5;
    float currentDepth = position.z;
    float bias = max(0.01 * (1.0 - dot(normal, lightDir)), minBias);
    int sampleCount = 7;
    float shadowRadius = 4000;

    float shadow = 0.0;
    for (int i = 0; i < sampleCount; i++) {
        int index = int(16 * random(coords.xyz, i)) % 16;
        float bufferDepth = texture(shadowSampler, vec2(position.x + poissonDisk[index].x / shadowRadius, 1.0 - (position.y + poissonDisk[index].y / shadowRadius))).r;
        shadow += (currentDepth - bias) > bufferDepth ? 1.0 : 0.0;
    }
    shadow /= sampleCount;
    if(position.z > 1.0)
        shadow = 0.0;
    return shadow;
}

float calculateTextureShadowDirectionalRefined(sampler2D shadowSampler, vec4 coords, vec3 normal, vec3 lightDir, float minBias) {
    // Perform perspective divide
    vec3 position = coords.xyz / coords.w;
    // Transform to [0,1] range
    position.xy = position.xy * 0.5 + 0.5;
    float currentDepth = position.z;
    float bias = max(0.01 * (1.0 - dot(normal, lightDir)), minBias);
    
    vec2 unitSize = 1.0 / textureSize(shadowSampler, 0); // Size of one texel in shadow map
    int sampleCount = 3; // Number of PCF samples per axis
    float filterRadius = 0.3; // Adjust this to control the softness of shadows

    float shadow = 0.0;
    // Iterate over a grid of samples around the current pixel
    for (int y = -sampleCount; y <= sampleCount; y++) {
        for (int x = -sampleCount; x <= sampleCount; x++) {
            vec2 offset = vec2(x, y) * unitSize * filterRadius;
            vec2 sampleCoord = vec2(position.x + offset.x, 1.0 - (position.y + offset.y)); // Flip Y
            float bufferDepth = texture(shadowSampler, sampleCoord).r;
            shadow += (currentDepth - bias) > bufferDepth ? 1.0 : 0.0;
        }
    }

    int totalSamples = (2 * sampleCount + 1) * (2 * sampleCount + 1); // Total samples in the grid
    shadow /= float(totalSamples); // Normalize the shadow value
    // Discard shadows outside the far plane
    if (position.z > 1.0) {
        shadow = 0.0;
    }
    return shadow;
}

float calculateTextureShadowDirectionalSimple(sampler2D shadowSampler, vec4 coords, vec3 normal, vec3 lightDir, float minBias) {
    // perform perspective divide, 
    vec3 position = coords.xyz / coords.w;
    // transform to [0,1] range
    position.xy = position.xy * 0.5 + 0.5;
    float currentDepth = position.z;
    float bias = max(0.01 * (1.0 - dot(normal, lightDir)), minBias);
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

float calculateTextureShadowDirectional(sampler2D shadowSampler, vec4 coords, vec3 normal, vec3 lightDir, float minBias) {
    return calculateTextureShadowDirectionalRefined(shadowSampler, coords, normal, lightDir, minBias);
}

vec3 sampleOffsetDirections[20] = vec3[]
(
   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);   

float calculateTextureShadowPointRefined(samplerCube shadowSampler, vec3 fragPosition, vec3 lightPosition, float far, float bias) {
    vec3 fragToLight = fragPosition - lightPosition;
    float currentDepth = length(fragToLight);
    float shadow = 0.0;

    float viewDistance = length(push.cameraPosition - fragPosition);
    float diskRadius = 0.01 + 0.1 * log(1.0 + viewDistance / far);
    int dynamicSamples = int(clamp(16.0 + (viewDistance / far) * 16.0, 16.0, 64.0));
    for (int i = 0; i < dynamicSamples; i++) {
        // use constant shift
        vec3 jitter = sampleOffsetDirections[(i + int(fragPosition.x * 10.0 + fragPosition.y * 10.0)) % 20] * diskRadius;
        float closestDepth = texture(shadowSampler, fragToLight + jitter).r;
        closestDepth *= far;
        if (currentDepth - bias > closestDepth) shadow += 1.0;
    }

    shadow /= float(dynamicSamples);
    return shadow;
}

float calculateTextureShadowPointSimple(samplerCube shadowSampler, vec3 fragPosition, vec3 lightPosition, float far, float bias) {
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

float calculateTextureShadowPoint(samplerCube shadowSampler, vec3 fragPosition, vec3 lightPosition, float far, float bias) {
    return calculateTextureShadowPointRefined(shadowSampler, fragPosition, lightPosition, far, bias);
}