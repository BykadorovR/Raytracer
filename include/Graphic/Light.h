#pragma once
#include "glm/glm.hpp"
#include <memory>

class PointLight {
 private:
  struct PhongLightFields {
    float ambient;
    float specular;
    alignas(16) glm::vec3 color;
    alignas(16) glm::vec3 position;
  };
  std::shared_ptr<PhongLightFields> _phong = nullptr;

 public:
  PointLight();
  void createPhong(float ambient, float specular, glm::vec3 color);
  void setPosition(glm::vec3 position);
  glm::vec3 getPosition();
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