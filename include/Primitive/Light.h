#pragma once
#include "glm/glm.hpp"
#include <memory>

struct LightFields {
  alignas(16) glm::vec3 color;
  alignas(16) glm::vec3 position;
};

class Light {
 protected:
  LightFields _lightFields;

 public:
  virtual void draw();
  virtual int getSize() = 0;
  virtual void* getData() = 0;
  void setPosition(glm::vec3 position);
  glm::vec3 getPosition();
  void setColor(glm::vec3 color);
  glm::vec3 getColor();
  virtual ~Light() = default;
};

struct PhongLightFields {
  float ambient;
  float specular;
  alignas(16) glm::vec3 color;
  alignas(16) glm::vec3 position;
};

class PhongLight : public Light {
 private:
  PhongLightFields _phongFields;

 public:
  void setAmbient(float ambient);
  void setSpecular(float specular);
  virtual int getSize() override;
  virtual void* getData() override;
};