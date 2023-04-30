#include "Light.h"

int PointLight::getSize() { return sizeof(PhongLightFields); }

PointLight::PointLight() { _phong = std::make_shared<PhongLightFields>(); }

void* PointLight::getData() { return _phong.get(); }

void PointLight::createPhong(float ambient, float specular, glm::vec3 color) {
  _phong->ambient = ambient;
  _phong->specular = specular;
  _phong->color = color;
}

void PointLight::setPosition(glm::vec3 position) { _phong->position = position; }

glm::vec3 PointLight::getPosition() { return _phong->position; }

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

void DirectionalLight::setDirection(glm::vec3 direction) { _phong->direction = direction; }

glm::vec3 DirectionalLight::getDirection() { return _phong->direction; }