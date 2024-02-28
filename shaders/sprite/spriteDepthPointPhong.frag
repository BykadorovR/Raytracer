#version 450
#define epsilon 0.0001 

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 texCoords;
layout(location = 2) in vec4 modelCoords;

layout(set = 1, binding = 0) uniform sampler2D texSampler;
layout(set = 1, binding = 1) uniform sampler2D normalSampler;
layout(set = 1, binding = 2) uniform sampler2D specularSampler;

layout( push_constant ) uniform constants {
    layout(offset = 0) vec3 lightPosition;
    layout(offset = 16) int far;
} PushConstants;

void main() {
    vec4 color = texture(texSampler, texCoords) * vec4(fragColor, 1.0);
    if (color.a < epsilon) {
        gl_FragDepth = 1.0;
    } else {
	   float lightDistance = length(modelCoords.xyz - PushConstants.lightPosition);
        
       // map to [0;1] range by dividing by far_plane
       lightDistance = lightDistance / PushConstants.far;
        
       // write this as modified depth
       gl_FragDepth = lightDistance;
    }
}