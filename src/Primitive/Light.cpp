#include "Light.h"

void Light::draw() {}

PhongLight::PhongLight() { _phongFields = std::make_shared<PhongLightFields>(); }

void PhongLight::setAmbient(float ambient) { _phongFields->ambient = ambient; }

void PhongLight::setSpecular(float specular) { _phongFields->specular = specular; }

void PhongLight::setPosition(glm::vec3 position) { _phongFields->position = position; }

void PhongLight::setColor(glm::vec3 color) { _phongFields->color = color; }

glm::vec3 PhongLight::getPosition() { return _phongFields->position; }

int PhongLight::getSize() { return sizeof(PhongLightFields); }

void* PhongLight::getData() { return _phongFields.get(); }