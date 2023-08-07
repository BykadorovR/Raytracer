#pragma once
#include <memory>
#include <vector>
#include "Texture.h"
#include "Cubemap.h"
#include "Settings.h"

class PointLight {
 private:
  struct PhongLightFields {
    float ambient;
    float specular;
    // attenuation
    float constant = 1.f;
    float linear = 0.f;
    float quadratic = 0.f;
    int distance;
    // parameters
    float far;
    // if alignment changed need to change implementation, sizeof(glm::vec4) is used in LightManager
    alignas(16) glm::vec3 color;
    alignas(16) glm::vec3 position;
  };
  std::shared_ptr<Settings> _settings;
  std::shared_ptr<PhongLightFields> _phong = nullptr;
  std::vector<std::shared_ptr<Cubemap>> _depthCubemap;
  int _attenuationIndex = 4;

 public:
  PointLight(std::shared_ptr<Settings> settings);
  void createPhong(float ambient, float specular, glm::vec3 color);
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
  void* getData();
};

class DirectionalLight {
 private:
  struct PhongLightFields {
    float ambient;
    float specular;
    alignas(16) glm::vec3 color;
    alignas(16) glm::vec3 position;
  };
  std::shared_ptr<PhongLightFields> _phong = nullptr;
  glm::vec3 _center, _up;
  std::vector<std::shared_ptr<Texture>> _depthTexture;

 public:
  DirectionalLight();
  void createPhong(float ambient, float specular, glm::vec3 color);
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