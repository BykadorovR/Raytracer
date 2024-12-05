#version 450
layout(location = 0) in vec4 modelCoords;
layout(location = 0) out vec4 outColor;

void main() {
    float dx = dFdx(gl_FragCoord.z);
    float dy = dFdy(gl_FragCoord.z);
    outColor = vec4(gl_FragCoord.z, gl_FragCoord.z * gl_FragCoord.z + 0.25 * (dx * dx + dy * dy), 0.0, 1.0);
}