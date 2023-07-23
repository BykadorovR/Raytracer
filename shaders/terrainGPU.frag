#version 450

layout(location = 0) in float fragHeight;
layout(location = 1) in vec2 texCoord;
layout(location = 0) out vec4 outColor;
layout(set = 3, binding = 0) uniform sampler2D terrainSampler[4];

float height0 = 32;
float height1 = 128;
float height2 = 192;
float height3 = 256; 

vec4 calculateColor(float max1, float max2, int id1, int id2, float height) {
    vec4 firstTile = texture(terrainSampler[id1], texCoord);
    vec4 secondTile = texture(terrainSampler[id2], texCoord);
    float delta = max2 - max1;
    float factor = (height - max1) / delta;
    return mix(firstTile, secondTile, factor);
}

void main() {
    float height = fragHeight;
    if (height < height0) {
        outColor = texture(terrainSampler[0], texCoord);
    } else if (height < height1) {
        outColor = calculateColor(height0, height1, 0, 1, height);
    } else if (height < height2) {
        outColor = calculateColor(height1, height2, 1, 2, height);
    } else if (height < height3) {
        outColor = calculateColor(height2, height3, 2, 3, height);
    }
}