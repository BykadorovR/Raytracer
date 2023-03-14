#pragma once
#include "glm/glm.hpp"

struct UniformObjectCamera {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 projection;
  alignas(16) glm::vec3 position;
};

struct UniformObjectPointLight {
  alignas(16) glm::vec3 position;
  alignas(16) glm::vec3 color;
  float radius;
};

struct UniformObjectLights {
  int number;
  UniformObjectPointLight pLights[16];
};