#version 450
layout(location = 0) in vec4 modelCoords;
layout(location = 0) out vec4 outColor;

layout( push_constant ) uniform constants {
    layout(offset = 0) vec3 lightPosition;
    layout(offset = 16) int far;
} PushConstants;

void main() {
	 float lightDistance = length(modelCoords.xyz - PushConstants.lightPosition);
    
    // map to [0;1] range by dividing by far_plane
    lightDistance = lightDistance / PushConstants.far;
    
    // write this as modified depth
    outColor = vec4(lightDistance, lightDistance * lightDistance, 0.0, 1.0);
}