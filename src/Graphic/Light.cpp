#include "Light.h"

int PointLight::getSize() { return sizeof(PhongLightFields); }

PointLight::PointLight(std::shared_ptr<Settings> settings) {
  _phong = std::make_shared<PhongLightFields>();
  _phong->far = 100.f;
  _settings = settings;

  setAttenuationIndex(_attenuationIndex);
}

void* PointLight::getData() { return _phong.get(); }

float PointLight::getFar() { return _phong->far; }

void PointLight::createPhong(glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, glm::vec3 color) {
  _phong->ambient = ambient;
  _phong->diffuse = diffuse;
  _phong->specular = specular;
  _phong->color = color;
}

void PointLight::setPosition(glm::vec3 position) { _phong->position = position; }

glm::vec3 PointLight::getPosition() { return _phong->position; }

glm::mat4 PointLight::getViewMatrix(int face) {
  // up is inverted because of some specific cubemap Y coordinate stuff
  auto viewMatrix = glm::mat4(1.f);
  switch (face) {
    case 0:  // POSITIVE_X / right
      viewMatrix = glm::lookAt(_phong->position, _phong->position + glm::vec3(1.0, 0.0, 0.0),
                               glm::vec3(0.0, -1.0, 0.0));
      break;
    case 1:  // NEGATIVE_X /left
      viewMatrix = glm::lookAt(_phong->position, _phong->position + glm::vec3(-1.0, 0.0, 0.0),
                               glm::vec3(0.0, -1.0, 0.0));
      break;
    case 2:  // POSITIVE_Y / top
      viewMatrix = glm::lookAt(_phong->position, _phong->position + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0));
      break;
    case 3:  // NEGATIVE_Y / bottom
      viewMatrix = glm::lookAt(_phong->position, _phong->position + glm::vec3(0.0, -1.0, 0.0),
                               glm::vec3(0.0, 0.0, -1.0));
      break;
    case 4:  // POSITIVE_Z / near
      viewMatrix = glm::lookAt(_phong->position, _phong->position + glm::vec3(0.0, 0.0, 1.0),
                               glm::vec3(0.0, -1.0, 0.0));
      break;
    case 5:  // NEGATIVE_Z / far
      viewMatrix = glm::lookAt(_phong->position, _phong->position + glm::vec3(0.0, 0.0, -1.0),
                               glm::vec3(0.0, -1.0, 0.0));
      break;
  }

  return viewMatrix;
}

glm::mat4 PointLight::getProjectionMatrix() {
  float aspect = 1.f;
  return glm::perspective(glm::radians(90.f), aspect, 0.1f, _phong->far);
}

void PointLight::setDepthCubemap(std::vector<std::shared_ptr<Cubemap>> depthCubemap) { _depthCubemap = depthCubemap; }

std::vector<std::shared_ptr<Cubemap>> PointLight::getDepthCubemap() { return _depthCubemap; }

int PointLight::getAttenuationIndex() { return _attenuationIndex; }

void PointLight::setAttenuationIndex(int index) {
  _attenuationIndex = index;
  _phong->distance = std::get<0>(_settings->getAttenuations()[index]);
  _phong->quadratic = std::get<1>(_settings->getAttenuations()[index]);
}

int PointLight::getDistance() { return _phong->distance; }

DirectionalLight::DirectionalLight() { _phong = std::make_shared<PhongLightFields>(); }

int DirectionalLight::getSize() { return sizeof(PhongLightFields); }

void* DirectionalLight::getData() { return _phong.get(); }

void DirectionalLight::createPhong(glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, glm::vec3 color) {
  _phong->ambient = ambient;
  _phong->diffuse = diffuse;
  _phong->specular = specular;
  _phong->color = color;
}

void DirectionalLight::setDepthTexture(std::vector<std::shared_ptr<Texture>> depthTexture) {
  _depthTexture = depthTexture;
}

std::vector<std::shared_ptr<Texture>> DirectionalLight::getDepthTexture() { return _depthTexture; }

void DirectionalLight::setPosition(glm::vec3 position) { _phong->position = position; }

glm::vec3 DirectionalLight::getPosition() { return _phong->position; }

void DirectionalLight::setCenter(glm::vec3 center) { _center = center; }

void DirectionalLight::setUp(glm::vec3 up) { _up = up; }

glm::mat4 DirectionalLight::getViewMatrix() { return glm::lookAt(_phong->position, _center, _up); }

glm::mat4 DirectionalLight::getProjectionMatrix() { return glm::ortho(-20.f, 20.f, -20.f, 20.f, 0.1f, 40.f); }