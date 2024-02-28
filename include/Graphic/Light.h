#pragma once
#include <memory>
#include <vector>
#include "Texture.h"
#include "Cubemap.h"
#include "Settings.h"

class PointLight {
 private:
  struct LightFields {
    // attenuation
    float quadratic = 0.f;
    int distance;
    // parameters
    float far;
    // if alignment changed need to change implementation, sizeof(glm::vec4) is used in LightManager
    alignas(16) glm::vec3 color;
    alignas(16) glm::vec3 position;
  };
  std::shared_ptr<Settings> _settings;
  std::shared_ptr<LightFields> _light = nullptr;
  std::vector<std::shared_ptr<Cubemap>> _depthCubemap;
  int _attenuationIndex = 4;

 public:
  PointLight(std::shared_ptr<Settings> settings);
  void setColor(glm::vec3 color);
  glm::vec3 getColor();
  void setDepthCubemap(std::vector<std::shared_ptr<Cubemap>> depthCubemap);
  std::vector<std::shared_ptr<Cubemap>> getDepthCubemap();
  void setPosition(glm::vec3 position);
  glm::vec3 getPosition();
  glm::mat4 getViewMatrix(int face);
  glm::mat4 getProjectionMatrix();
  void setAttenuationIndex(int index);
  int getAttenuationIndex();
  int getDistance();
  int getSize();
  float getFar();
  void* getData();
};

class DirectionalLight {
 private:
  struct LightFields {
    alignas(16) glm::vec3 color;
    alignas(16) glm::vec3 position;
  };
  std::shared_ptr<LightFields> _light = nullptr;
  glm::vec3 _center, _up;
  std::vector<std::shared_ptr<Texture>> _depthTexture;

 public:
  DirectionalLight();
  void setColor(glm::vec3 color);
  glm::vec3 getColor();
  void setDepthTexture(std::vector<std::shared_ptr<Texture>> depthTexture);
  std::vector<std::shared_ptr<Texture>> getDepthTexture();
  void setPosition(glm::vec3 position);
  void setCenter(glm::vec3 center);
  void setUp(glm::vec3 up);
  glm::vec3 getPosition();
  glm::mat4 getViewMatrix();
  glm::mat4 getProjectionMatrix();
  int getSize();
  void* getData();
};

class AmbientLight {
 private:
  struct LightFields {
    alignas(16) glm::vec3 color;
  };

  std::shared_ptr<LightFields> _light = nullptr;

 public:
  AmbientLight();
  void setColor(glm::vec3 color);
  int getSize();
  void* getData();
};