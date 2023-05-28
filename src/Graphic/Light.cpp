#include "Light.h"
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

int PointLight::getSize() { return sizeof(PhongLightFields); }

PointLight::PointLight(std::shared_ptr<Settings> settings) {
  _phong = std::make_shared<PhongLightFields>();
  _settings = settings;
}

void* PointLight::getData() { return _phong.get(); }

void PointLight::createPhong(float ambient, float specular, glm::vec3 color) {
  _phong->ambient = ambient;
  _phong->specular = specular;
  _phong->color = color;
}

void PointLight::setPosition(glm::vec3 position) { _phong->position = position; }

glm::vec3 PointLight::getPosition() { return _phong->position; }

glm::mat4 PointLight::getViewMatrix(int face) {
  glm::mat4 viewMatrix = glm::mat4(1.0f);
  switch (face) {
    case 0:  // POSITIVE_X
      viewMatrix = glm::rotate(viewMatrix, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
      viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
      break;
    case 1:  // NEGATIVE_X
      viewMatrix = glm::rotate(viewMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
      viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
      break;
    case 2:  // POSITIVE_Y
      viewMatrix = glm::rotate(viewMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
      break;
    case 3:  // NEGATIVE_Y
      viewMatrix = glm::rotate(viewMatrix, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
      break;
    case 4:  // POSITIVE_Z
      viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
      break;
    case 5:  // NEGATIVE_Z
      viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
      break;
  }

  return viewMatrix;
}

glm::mat4 PointLight::getProjectionMatrix() {
  float aspect = std::get<0>(_settings->getResolution()) / std::get<1>(_settings->getResolution());
  return glm::perspective(glm::radians(90.f), aspect, 0.1f, 40.f);
}

void PointLight::setDepthCubemap(std::vector<std::shared_ptr<Cubemap>> depthCubemap) { _depthCubemap = depthCubemap; }

std::vector<std::shared_ptr<Cubemap>> PointLight::getDepthCubemap() { return _depthCubemap; }

void PointLight::setAttenuation(float constant, float linear, float quadratic) {
  _phong->constant = constant;
  _phong->linear = linear;
  _phong->quadratic = quadratic;
}

DirectionalLight::DirectionalLight() { _phong = std::make_shared<PhongLightFields>(); }

int DirectionalLight::getSize() { return sizeof(PhongLightFields); }

void* DirectionalLight::getData() { return _phong.get(); }

void DirectionalLight::createPhong(float ambient, float specular, glm::vec3 color) {
  _phong->ambient = ambient;
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

glm::mat4 DirectionalLight::getProjectionMatrix() { return glm::ortho(-10.f, 10.f, -10.f, 10.f, 0.1f, 40.f); }