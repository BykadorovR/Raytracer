#include "Graphic/Light.h"

int PointLight::getSize() { return sizeof(LightFields); }

PointLight::PointLight(std::shared_ptr<Settings> settings) {
  _light = std::make_shared<LightFields>();
  _light->far = 100.f;
  _settings = settings;

  setAttenuationIndex(_attenuationIndex);
}

void* PointLight::getData() { return _light.get(); }

float PointLight::getFar() { return _light->far; }

void PointLight::setColor(glm::vec3 color) { _light->color = color; }

glm::vec3 PointLight::getColor() { return _light->color; }

void PointLight::setPosition(glm::vec3 position) { _light->position = position; }

glm::vec3 PointLight::getPosition() { return _light->position; }

glm::mat4 PointLight::getViewMatrix(int face) {
  // up is inverted for X and Z because of some specific cubemap Y coordinate stuff
  auto viewMatrix = glm::mat4(1.f);
  switch (face) {
    case 0:  // POSITIVE_X / right
      viewMatrix = glm::lookAt(_light->position, _light->position + glm::vec3(1.0, 0.0, 0.0),
                               glm::vec3(0.0, -1.0, 0.0));
      break;
    case 1:  // NEGATIVE_X /left
      viewMatrix = glm::lookAt(_light->position, _light->position + glm::vec3(-1.0, 0.0, 0.0),
                               glm::vec3(0.0, -1.0, 0.0));
      break;
    case 2:  // POSITIVE_Y / top
      viewMatrix = glm::lookAt(_light->position, _light->position + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0));
      break;
    case 3:  // NEGATIVE_Y / bottom
      viewMatrix = glm::lookAt(_light->position, _light->position + glm::vec3(0.0, -1.0, 0.0),
                               glm::vec3(0.0, 0.0, -1.0));
      break;
    case 4:  // POSITIVE_Z / near
      viewMatrix = glm::lookAt(_light->position, _light->position + glm::vec3(0.0, 0.0, 1.0),
                               glm::vec3(0.0, -1.0, 0.0));
      break;
    case 5:  // NEGATIVE_Z / far
      viewMatrix = glm::lookAt(_light->position, _light->position + glm::vec3(0.0, 0.0, -1.0),
                               glm::vec3(0.0, -1.0, 0.0));
      break;
  }

  return viewMatrix;
}

glm::mat4 PointLight::getProjectionMatrix() {
  float aspect = 1.f;
  return glm::perspective(glm::radians(90.f), aspect, 0.1f, _light->far);
}

void PointLight::setDepthCubemap(std::vector<std::shared_ptr<Cubemap>> depthCubemap) { _depthCubemap = depthCubemap; }

std::vector<std::shared_ptr<Cubemap>> PointLight::getDepthCubemap() { return _depthCubemap; }

int PointLight::getAttenuationIndex() { return _attenuationIndex; }

void PointLight::setAttenuationIndex(int index) {
  _attenuationIndex = index;
  _light->distance = std::get<0>(_settings->getAttenuations()[index]);
  _light->quadratic = std::get<1>(_settings->getAttenuations()[index]);
}

int PointLight::getDistance() { return _light->distance; }

DirectionalLight::DirectionalLight() {
  _light = std::make_shared<LightFields>();
  _center = glm::vec3(0.f, 0.f, 0.f);
  _up = glm::vec3(0.f, 0.f, -1.f);
}

int DirectionalLight::getSize() { return sizeof(LightFields); }

void* DirectionalLight::getData() { return _light.get(); }

void DirectionalLight::setColor(glm::vec3 color) { _light->color = color; }

glm::vec3 DirectionalLight::getColor() { return _light->color; }

void DirectionalLight::setDepthTexture(std::vector<std::shared_ptr<Texture>> depthTexture) {
  _depthTexture = depthTexture;
}

std::vector<std::shared_ptr<Texture>> DirectionalLight::getDepthTexture() { return _depthTexture; }

void DirectionalLight::setPosition(glm::vec3 position) { _light->position = position; }

glm::vec3 DirectionalLight::getPosition() { return _light->position; }

void DirectionalLight::setCenter(glm::vec3 center) { _center = center; }

void DirectionalLight::setUp(glm::vec3 up) { _up = up; }

glm::mat4 DirectionalLight::getViewMatrix() { return glm::lookAt(_light->position, _center, _up); }

glm::mat4 DirectionalLight::getProjectionMatrix() { return glm::ortho(-20.f, 20.f, -20.f, 20.f, 0.1f, 40.f); }

AmbientLight::AmbientLight() { _light = std::make_shared<LightFields>(); }

void AmbientLight::setColor(glm::vec3 color) { _light->color = color; }

int AmbientLight::getSize() { return sizeof(LightFields); }

void* AmbientLight::getData() { return _light.get(); }