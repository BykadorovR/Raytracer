#version 450
layout(location = 0) in vec4 modelCoords;

layout( push_constant ) uniform constants {
    layout(offset = 16) vec3 lightPosition;
    int far;
} PushConstants;

void main() {
	 float lightDistance = length(modelCoords.xyz - PushConstants.lightPosition);
    
    // map to [0;1] range by dividing by far_plane
    lightDistance = lightDistance / PushConstants.far;
    
    // write this as modified depth
    gl_FragDepth = lightDistance;
}