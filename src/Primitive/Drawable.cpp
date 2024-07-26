#include "Drawable.h"

void Named::setName(std::string name) { _name = name; }

std::string Named::getName() { return _name; }

void Drawable::setModel(glm::mat4 model) { _model = model; }

glm::mat4 Drawable::getModel() { return _model; }