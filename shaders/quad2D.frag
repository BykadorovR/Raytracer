#version 450
layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;
layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout( push_constant ) uniform constants {
    float near;
    float far;
} push;

float linearizeDepth(float depth)
{
  float n = push.near;
  float f = push.far;
  float z = depth;
  return (2.0 * n) / (f + n - z * (f - n)); 
}

void main() {
    float depth = texture(texSampler, fragTexCoord).r;
    //outColor = vec4(vec3(linearizeDepth(depth)), 1.f);
    outColor = vec4(vec3(depth), 1.f);
}