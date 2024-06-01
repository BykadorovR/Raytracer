#version 450

layout(location = 0) in vec3 TexCoords;
layout(set = 0, binding = 1) uniform sampler2D skybox;

layout(location = 0) out vec4 outColor;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v) {
    //in Vulkan image is going from top to down, so image "up" vector is -v.y, not v.y as in OpenGL
    vec2 uv = vec2(atan(v.z, v.x), asin(-v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {
    vec2 uv = SampleSphericalMap(normalize(TexCoords));
    vec3 color = texture(skybox, vec2(uv.x, uv.y)).rgb;
    
    outColor = vec4(color, 1.0);
}