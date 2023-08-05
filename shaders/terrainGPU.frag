#version 450

layout(location = 0) in float fragHeight;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 tessColor;
layout(location = 0) out vec4 outColor;
layout(set = 3, binding = 0) uniform sampler2D terrainSampler[4];

layout( push_constant ) uniform constants {
    layout(offset = 32) float heightLevels[4];
    int patchEdge;
    int tessColorFlag;
} push;

//It is important to get the gradients before going into non-uniform flow code.
vec2 dx = dFdx(texCoord);
vec2 dy = dFdy(texCoord);
vec4 calculateColor(float max1, float max2, int id1, int id2, float height) {
    vec4 firstTile = textureGrad(terrainSampler[id1], texCoord, dx, dy);
    vec4 secondTile = textureGrad(terrainSampler[id2], texCoord, dx, dy);
    float delta = max2 - max1;
    float factor = (height - max1) / delta;
    return mix(firstTile, secondTile, factor);
}

void main() {
    if (push.tessColorFlag > 0) {
        outColor = vec4(tessColor, 1);
    } else {
        float height = fragHeight;
        if (height < push.heightLevels[0]) {
            outColor = texture(terrainSampler[0], texCoord);
        } else if (height < push.heightLevels[1]) {
            outColor = calculateColor(push.heightLevels[0], push.heightLevels[1], 0, 1, height);
        } else if (height < push.heightLevels[2]) {
            outColor = calculateColor(push.heightLevels[1], push.heightLevels[2], 1, 2, height);
        } else if (height < push.heightLevels[3]) {
            outColor = calculateColor(push.heightLevels[2], push.heightLevels[3], 2, 3, height);
        } else {
            outColor = texture(terrainSampler[3], texCoord);
        }
    }

    vec2 line = fract(texCoord);
    if (push.patchEdge > 0 && (line.x < 0.001 || line.y < 0.001 || line.x > 0.999 || line.y > 0.999)) {
        outColor = vec4(1, 1, 0, 1);
    }
}