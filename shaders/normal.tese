// tessellation evaluation shader
#version 450 core

layout (quads, fractional_odd_spacing, ccw) in;

// received from Tessellation Control Shader - all texture coordinates for the patch vertices
layout (location = 0) in vec2 TextureCoord[];
layout(set = 1, binding = 0) uniform UniformCamera {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

layout(set = 2, binding = 0) uniform sampler2D heightMap;

layout (location = 0) out vec3 normalVertex;
// send to Fragment Shader for coloring
layout (location = 1) out vec3 colorVertex;

layout( push_constant ) uniform constants {
    layout(offset = 16) int patchDimX;
    int patchDimY;
    float heightScale;
    float heightShift;
} push;

void main()
{
    // get patch coordinate (2D)
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    // ----------------------------------------------------------------------
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
    vec2 texCoord = TexCoord / vec2(push.patchDimX, push.patchDimY);
    float heightValue = texture(heightMap, texCoord).y;
    //
    float left = textureOffset(heightMap, texCoord, ivec2(-1, 0)).y * push.heightScale - push.heightShift;
    float right = textureOffset(heightMap, texCoord, ivec2(1, 0)).y * push.heightScale - push.heightShift;
    float top = textureOffset(heightMap, texCoord, ivec2(0, 1)).y * push.heightScale - push.heightShift;
    float bottom = textureOffset(heightMap, texCoord, ivec2(0, -1)).y * push.heightScale - push.heightShift;
    //TODO: change 2.0 to real size
    //gl_TessLevelInner[0] - x, gl_TessLevelInner[1] - z
    //12 patches
    vec2 textureSize = textureSize(heightMap, 0);
    vec2 stepSize = textureSize / (vec2(push.patchDimX, push.patchDimY) * vec2(gl_TessLevelInner[0], gl_TessLevelInner[1]));

    vec3 tangent = vec3(2.0 * stepSize.x, right - left, 0.0);
    vec3 bitangent = vec3(0.0, bottom - top, -2.0 * stepSize.y);
    colorVertex = normalize(cross(tangent, bitangent));
    normalVertex = normalize(vec3(mvp.view * vec4(colorVertex, 0.0)));
    //

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

    // displace point along normal
    p += normal * (heightValue * push.heightScale - push.heightShift);

    // ----------------------------------------------------------------------
    // output patch point position in clip space
    gl_Position = mvp.view * mvp.model * p;
}