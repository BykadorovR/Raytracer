#pragma once
#include "glm/glm.hpp"
#include <memory>

class Light {
 public:
  virtual void draw();
  virtual int getSize() = 0;
  virtual void* getData() = 0;
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
  std::shared_ptr<PhongLightFields> _phongFields;

 public:
  PhongLight();
  void setPosition(glm::vec3 position);
  glm::vec3 getPosition();
  void setColor(glm::vec3 color);
  void setAmbient(float ambient);
  void setSpecular(float specular);

  virtual int getSize() override;
  virtual void* getData() override;
};