// tessellation evaluation shader
#version 450 core

layout (quads, fractional_odd_spacing, cw) in;

// received from Tessellation Control Shader - all texture coordinates for the patch vertices
layout (location = 0) in vec2 TextureCoord[];

layout(set = 0, binding = 1) uniform UniformCamera {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

layout(set = 0, binding = 2) uniform sampler2D heightMap;
layout(location = 0) out vec4 modelCoords;

layout( push_constant ) uniform constants {
    layout(offset = 16) int patchDimX;
    int patchDimY;
    float heightScale;
    float heightShift;
} push;

float calculateHeightTexture(in vec2 TexCoord, inout vec2 texCoord) {
    texCoord = TexCoord / vec2(push.patchDimX, push.patchDimY);
    return texture(heightMap, texCoord).x;
}

float calculateHeightPosition(in vec2 p, inout vec2 texCoord) {
    vec2 textureSize = textureSize(heightMap, 0);
    vec2 pos = p + (textureSize - vec2(1.0)) / vec2(2.0);
    ivec2 integral = ivec2(floor(pos));
    texCoord = fract(pos);
    ivec2 index0 = integral;
    ivec2 index1 = integral + ivec2(1, 0);
    ivec2 index2 = integral + ivec2(0, 1);
    ivec2 index3 = integral + ivec2(1, 1);
    float sample0 = texelFetch(heightMap, index0, 0).x;
    float sample1 = texelFetch(heightMap, index1, 0).x;
    float sample2 = texelFetch(heightMap, index2, 0).x;
    float sample3 = texelFetch(heightMap, index3, 0).x;
    float fxy1 = sample0 + texCoord.x * (sample1 - sample0);
    float fxy2 = sample2 + texCoord.x * (sample3 - sample2);
    float heightValue = fxy1 + texCoord.y * (fxy2 - fxy1);
    return heightValue;
}

void main() {
    // get patch coordinate (2D)
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    // retrieve control point texture coordinates
    vec2 t00 = TextureCoord[0];
    vec2 t01 = TextureCoord[1];
    vec2 t10 = TextureCoord[2];
    vec2 t11 = TextureCoord[3];

    // bilinearly interpolate texture coordinate across patch
    vec2 t0 = (t01 - t00) * u + t00;
    vec2 t1 = (t11 - t10) * u + t10;
    vec2 TexCoord = (t1 - t0) * v + t0;
    // IMPORTANT: need to divide, otherwise we will have the whole heightmap for every tile
    // lookup texel at patch coordinate for height and scale + shift as desired

    // ----------------------------------------------------------------------
    // retrieve control point position coordinates
    vec4 p00 = gl_in[0].gl_Position;
    vec4 p01 = gl_in[1].gl_Position;
    vec4 p10 = gl_in[2].gl_Position;
    vec4 p11 = gl_in[3].gl_Position;

    // compute patch surface normal
    vec4 uVec = p01 - p00;
    vec4 vVec = p10 - p00;
    vec4 normal = normalize( vec4(cross(vVec.xyz, uVec.xyz), 0) );

    // bilinearly interpolate position coordinate across patch
    vec4 p0 = (p01 - p00) * u + p00;
    vec4 p1 = (p11 - p10) * u + p10;
    vec4 p = (p1 - p0) * v + p0;

    vec2 texCoord;
    float heightValue = calculateHeightTexture(TexCoord, texCoord);
    // calculate the same way as in C++, but result is the same as in the line above
    //float heightValue = calculateHeightPosition(p.xz, texCoord);

    // displace point along normal
    p += normal * (heightValue * push.heightScale - push.heightShift);

    // ----------------------------------------------------------------------
    // output patch point position in clip space
    gl_Position = mvp.proj * mvp.view * mvp.model * p;
    modelCoords = mvp.model * p;
}