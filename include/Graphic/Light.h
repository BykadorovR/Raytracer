#pragma once
#include "glm/glm.hpp"
#include <memory>

class PointLight {
 private:
  struct PhongLightFields {
    float ambient;
    float specular;
    // attenuation
    float constant = 1.f;
    float linear = 0.f;
    float quadratic = 0.f;
    // if alignment changed need to change implementation, sizeof(glm::vec4) is used in LightManager
    alignas(16) glm::vec3 color;
    alignas(16) glm::vec3 position;
  };
  std::shared_ptr<PhongLightFields> _phong = nullptr;
  glm::mat4 _projection;

 public:
  PointLight();
  void createPhong(float ambient, float specular, glm::vec3 color);
  void setPosition(glm::vec3 position);
  glm::vec3 getPosition();
  void setAttenuation(float constant, float linear, float quadratic);
  int getSize();
  void* getData();
};

class DirectionalLight {
 private:
  struct PhongLightFields {
    float ambient;
    float specular;
    alignas(16) glm::vec3 color;
    alignas(16) glm::vec3 direction;
  };
  std::shared_ptr<PhongLightFields> _phong = nullptr;

 public:
  DirectionalLight();
  void createPhong(float ambient, float specular, glm::vec3 color);
  void setDirection(glm::vec3 direction);
  glm::vec3 getDirection();
  int getSize();
  void* getData();
};