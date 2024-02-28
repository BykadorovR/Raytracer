vec3 directionalLight(int lightDirectionalNumber, vec3 fragPosition, vec3 normal, float specularTexture, vec3 cameraPosition, 
                      int enableShadow, vec4 fragLightDirectionalCoord[2], sampler2D shadowDirectionalSampler[2], float bias) {
    vec3 lightFactor = vec3(0.0, 0.0, 0.0);
    for (int i = 0; i < lightDirectionalNumber; i++) {
        vec3 lightDir = normalize(getLightDir(i).position - fragPosition);
        float shadow = 0.0;
        if (enableShadow > 0)
            shadow = calculateTextureShadowDirectional(shadowDirectionalSampler[i], fragLightDirectionalCoord[i], normal, lightDir, bias); 
        //dot product between normal and light ray
        vec3 diffuseFactor = max(dot(lightDir, normal), 0) * getMaterial().diffuse;
        //dot product between reflected ray and light ray
        //Phong implementation
        /*vec3 specularFactor = getMaterial().specular * 
                               pow(max(dot(normalize(reflect(-lightDir, normal)), normalize(cameraPosition - fragPosition)), 0), max(getMaterial().shininess, 1));*/
        //Blinn-Phong implementation
        vec3 viewDir = normalize(cameraPosition - fragPosition);
        vec3 halfwayDir = normalize(viewDir + lightDir);
        vec3 specularFactor = specularTexture * getMaterial().specular * pow(max(dot(normal, halfwayDir), 0), max(getMaterial().shininess, 1));
        lightFactor += getLightDir(i).color * ((1 - shadow) * (diffuseFactor + specularFactor));
    }
    return lightFactor;
}

vec3 pointLight(int lightPointNumber, vec3 fragPosition, vec3 normal, float specularTexture, vec3 cameraPosition, 
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
        //dot product between normal and light ray
        vec3 diffuseFactor = max(dot(lightDir, normal), 0) * getMaterial().diffuse;
        //dot product between reflected ray and light ray
        //Phong implementation
        /*vec3 specularFactor = getMaterial().specular * 
                               pow(max(dot(normalize(reflect(-lightDir, normal)), normalize(cameraPosition - fragPosition)), 0), max(getMaterial().shininess, 1));*/
        //Blinn-Phong implementation
        vec3 viewDir = normalize(cameraPosition - fragPosition);
        vec3 halfwayDir = normalize(viewDir + lightDir);
        vec3 specularFactor = specularTexture * getMaterial().specular * pow(max(dot(normal, halfwayDir), 0), max(getMaterial().shininess, 1));
        lightFactor += getLightPoint(i).color * attenuation * ((1 - shadow) * (diffuseFactor + specularFactor)); 
    }

    return lightFactor;
}