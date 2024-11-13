// tessellation control shader
#version 450

// specify number of control points per patch output
// this value controls the size of the input and output arrays
layout (vertices = 4) out;

// varying input from vertex shader
layout (location = 0) in vec2 TexCoord[];

layout(set = 0, binding = 0) uniform UniformCamera {
    mat4 model;
    mat4 view;
    mat4 proj;
} mvp;

struct PatchDescription {
    int rotation;
    int textureID;
};

layout(std140, set = 0, binding = 1) readonly buffer PatchDescriptionBuffer {
    PatchDescription patchDescription[];
};

layout( push_constant ) uniform constants {
    int patchDimX;
    int patchDimY;
    int minTessellationLevel;
    int maxTessellationLevel;
    float near;
    float far;
} push;

// varying output to evaluation shader
layout (location = 0) out vec2 TextureCoord[];
layout (location = 1) out vec4 outTilesWeights[];
layout (location = 2) flat out int outTextureID[];
layout (location = 3) flat out ivec4 outNeighboursID[];
layout (location = 4) flat out int outRotation[];

// All components are in the range [0…1], including hue.
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

int getPatchID(int x, int y) {
    int newID = gl_PrimitiveID + x + y * push.patchDimX;
    newID = min(newID, push.patchDimX * push.patchDimY - 1);
    newID = max(newID, 0);
    return newID;
}

void main() {
    // ----------------------------------------------------------------------
    // pass attributes through
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    TextureCoord[gl_InvocationID] = TexCoord[gl_InvocationID];

    //set default level for tessColor
    gl_TessLevelOuter[gl_InvocationID] = push.minTessellationLevel;

    // ----------------------------------------------------------------------
    // invocation zero controls tessellation levels for the entire patch
    if (gl_InvocationID == 0) {
        // Step 1: transform each vertex into eye space
        vec4 eyeSpacePos00 = mvp.view * mvp.model * gl_in[0].gl_Position;
        vec4 eyeSpacePos01 = mvp.view * mvp.model * gl_in[1].gl_Position;
        vec4 eyeSpacePos10 = mvp.view * mvp.model * gl_in[2].gl_Position;
        vec4 eyeSpacePos11 = mvp.view * mvp.model * gl_in[3].gl_Position;

        // ----------------------------------------------------------------------
        // Step 2: "distance" from camera scaled between 0 and 1
        float distance00 = clamp((abs(eyeSpacePos00.z) - push.near) / (push.far - push.near), 0.0, 1.0);
        float distance01 = clamp((abs(eyeSpacePos01.z) - push.near) / (push.far - push.near), 0.0, 1.0);
        float distance10 = clamp((abs(eyeSpacePos10.z) - push.near) / (push.far - push.near), 0.0, 1.0);
        float distance11 = clamp((abs(eyeSpacePos11.z) - push.near) / (push.far - push.near), 0.0, 1.0);

        // ----------------------------------------------------------------------
        // Step 3: interpolate edge tessellation level based on closer vertex
        float tessLevel0 = mix( push.maxTessellationLevel, push.minTessellationLevel, min(distance10, distance00) );
        float tessLevel1 = mix( push.maxTessellationLevel, push.minTessellationLevel, min(distance00, distance01) );
        float tessLevel2 = mix( push.maxTessellationLevel, push.minTessellationLevel, min(distance01, distance11) );
        float tessLevel3 = mix( push.maxTessellationLevel, push.minTessellationLevel, min(distance11, distance10) );

        // ----------------------------------------------------------------------
        // Step 4: set the corresponding outer edge tessellation levels
        gl_TessLevelOuter[0] = tessLevel0;
        gl_TessLevelOuter[1] = tessLevel1;
        gl_TessLevelOuter[2] = tessLevel2;
        gl_TessLevelOuter[3] = tessLevel3;

        // ----------------------------------------------------------------------
        // Step 5: set the inner tessellation levels to the max of the two parallel edges
        gl_TessLevelInner[0] = max(tessLevel1, tessLevel3);
        gl_TessLevelInner[1] = max(tessLevel0, tessLevel2);
    }

    int offsetX = gl_InvocationID % 2;
    int offsetY = gl_InvocationID / 2;
    outTilesWeights[gl_InvocationID] = vec4(0, 0, 0, 0);
    // calculate weight of the texture in each specific vertex
    outTilesWeights[gl_InvocationID][patchDescription[getPatchID(-1 + offsetX, -1 + offsetY)].textureID] = 1;
    outTilesWeights[gl_InvocationID][patchDescription[getPatchID(0  + offsetX, -1 + offsetY)].textureID] = 1;
    outTilesWeights[gl_InvocationID][patchDescription[getPatchID(-1 + offsetX, 0  + offsetY)].textureID] = 1;
    outTilesWeights[gl_InvocationID][patchDescription[getPatchID(0  + offsetX, 0  + offsetY)].textureID] = 1;

    // pass current patch texture ID
    outTextureID[gl_InvocationID] = patchDescription[gl_PrimitiveID].textureID;
    // pass left - top - right - bottom texture ids of neighbours
    outNeighboursID[gl_InvocationID] = ivec4(patchDescription[getPatchID(-1, 0)].textureID,
                                             patchDescription[getPatchID(0, -1)].textureID,
                                             patchDescription[getPatchID(1, 0)].textureID,
                                             patchDescription[getPatchID(0, 1)].textureID);


    outRotation[gl_InvocationID] = patchDescription[gl_PrimitiveID].rotation;
}