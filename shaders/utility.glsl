float randomFloat(vec2 seed) {
    return fract(sin(dot(seed.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float randomFloat(float min, float max, vec2 seed) {
  return min + (max - min) * randomFloat(seed);
}