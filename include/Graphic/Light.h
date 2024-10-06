#pragma once
#include <memory>
#include <vector>
#include "Graphic/Texture.h"
#include "Primitive/Cubemap.h"
#include "Utility/Settings.h"
#include "Graphic/Camera.h"

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
  std::shared_ptr<CameraPointLight> _camera;

 public:
  PointLight(std::shared_ptr<Settings> settings);
  void setColor(glm::vec3 color);
  glm::vec3 getColor();
  void setDepthCubemap(std::vector<std::shared_ptr<Cubemap>> depthCubemap);
  std::vector<std::shared_ptr<Cubemap>> getDepthCubemap();
  std::shared_ptr<CameraPointLight> getCamera();
  void setAttenuationIndex(int index);
  int getAttenuationIndex();
  int getDistance();
  int getSize();
  void* getData();
};

class DirectionalLight {
 private:
  struct LightFields {
    alignas(16) glm::vec3 color;
    alignas(16) glm::vec3 position;
  };
  std::shared_ptr<LightFields> _light = nullptr;
  std::vector<std::shared_ptr<Texture>> _depthTexture;
  std::shared_ptr<CameraDirectionalLight> _camera;

 public:
  DirectionalLight();
  void setColor(glm::vec3 color);
  glm::vec3 getColor();
  void setDepthTexture(std::vector<std::shared_ptr<Texture>> depthTexture);
  std::vector<std::shared_ptr<Texture>> getDepthTexture();
  std::shared_ptr<CameraDirectionalLight> getCamera();
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