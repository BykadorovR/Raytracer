#version 450
layout(location = 0) out vec4 outColor;

void main() {
	outColor = vec4(gl_FragCoord.z, gl_FragCoord.z * gl_FragCoord.z, 0.0, 1.0);
}