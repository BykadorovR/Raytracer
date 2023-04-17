#include "Light.h"

void Light::draw() {}

void Light::setPosition(glm::vec3 position) { _lightFields.position = position; }

void Light::setColor(glm::vec3 color) { _lightFields.color = color; }

glm::vec3 Light::getColor() { return _lightFields.color; }

glm::vec3 Light::getPosition() { return _lightFields.position; }

void PhongLight::setAmbient(float ambient) { _phongFields.ambient = ambient; }

void PhongLight::setSpecular(float specular) { _phongFields.specular = specular; }

int PhongLight::getSize() { return sizeof(PhongLightFields); }

void* PhongLight::getData() {
  _phongFields.position = _lightFields.position;
  _phongFields.color = _lightFields.color;
  return &_phongFields;
}