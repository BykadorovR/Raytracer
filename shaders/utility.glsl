uint seed;

//https://github.com/GPSnoopy/RayTracingInVulkan/blob/master/assets/shaders/Random.glsl
uint initRandomSeed(uint val0, uint val1) {
  uint v0 = val0, v1 = val1, s0 = 0;

  for (uint n = 0; n < 16; n++) {
    s0 += 0x9e3779b9;
    v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
    v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
  }

  return v0;
}

uint randomInt() {
  // LCG values from Numerical Recipes
    return (seed = 1664525 * seed + 1013904223);
}

float randomFloat() {
  // Faster version from NVIDIA examples; quality good enough for our use case.
  return (float(randomInt() & 0x00FFFFFF) / float(0x01000000));
}

float randomFloat(float min, float max) {
  return min + (max - min) * randomFloat();
}

vec3 randomInUnitSphere() {
  for (;;) {
    const vec3 p = 2 * vec3(randomFloat(), randomFloat(), randomFloat()) - 1;
    if (dot(p, p) < 1)
    {
      return p;
    }
  }
}
