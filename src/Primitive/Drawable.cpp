#include "Primitive/Drawable.h"

void Named::setName(std::string name) { _name = name; }

std::string Named::getName() { return _name; }

void Drawable::setOriginShift(glm::vec3 originShift) { _originShift = originShift; }

void Drawable::setOriginRotate(glm::quat rotate) { _originRotate = rotate; }

void Drawable::setTranslate(glm::vec3 translate) { _translate = translate; }

void Drawable::setRotate(glm::quat rotate) { _rotate = rotate; }

void Drawable::setScale(glm::vec3 scale) { _scale = scale; }

glm::vec3 Drawable::getOriginShift() { return _originShift; }

glm::vec3 Drawable::getTranslate() { return _translate; }

glm::quat Drawable::getRotate() { return _rotate; }

glm::vec3 Drawable::getScale() { return _scale; }

glm::mat4 Drawable::getModel() {
  // goal is receive transformation matrix in form of translate * rotate * scale * originRotate * originTranslate *
  // position; to achieve this we have to apply matrices in this order: translate -> rotate -> scale -> originRotate ->
  // originTranslate
  auto matrix = glm::translate(glm::mat4(1.f), _translate);
  matrix = matrix * glm::mat4_cast(_rotate);
  matrix = glm::scale(matrix, _scale);
  // we want to apply rotate to already changed by shift origin
  matrix = matrix * glm::mat4_cast(_originRotate);
  matrix = glm::translate(matrix, _originShift);
  return matrix;
}