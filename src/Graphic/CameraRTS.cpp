#include "Graphic/CameraRTS.h"

CameraRTS::CameraRTS(std::shared_ptr<Drawable> object, std::shared_ptr<EngineState> engineState) {
  _object = object;
  _engineState = engineState;
  auto model = _object->getModel() * _object->getOrigin();
  _eye = glm::vec3(model[3][0], model[3][1], model[3][2]) + _shift;
  _direction = glm::normalize(glm::vec3(model[3][0], model[3][1], model[3][2]) - _eye);
  _up = glm::vec3(0.f, 0.f, -1.f);
}

void CameraRTS::setShift(glm::vec3 shift) { _shift = shift; }

void CameraRTS::setThreshold(int threshold) { _threshold = threshold; }

void CameraRTS::setProjectionParameters(float fov, float near, float far) {
  _fov = fov;
  _near = near;
  _far = far;
}

void CameraRTS::setAspect(float aspect) { _aspect = aspect; }

glm::mat4 CameraRTS::getView() {
  if (_attached) {
    auto model = _object->getModel() * _object->getOrigin();
    _eye = glm::vec3(model[3][0], model[3][1], model[3][2]) + _shift;
    _direction = glm::normalize(glm::vec3(model[3][0], model[3][1], model[3][2]) - _eye);
  }
  return glm::lookAt(_eye, _eye + _direction, _up);
}

glm::mat4 CameraRTS::getProjection() {
  auto projection = glm::perspective(glm::radians(_fov), _aspect, _near, _far);
  return projection;
}

void CameraRTS::cursorNotify(float xPos, float yPos) {
  auto [wW, wH] = _engineState->getWindow()->getResolution();
  if (xPos < _threshold || yPos < _threshold || xPos > wW - _threshold || yPos > wH - _threshold) {
    _attached = false;
    // need to move camera only if cursor is near the end of the window
    float xOffset = 0.f;
    if (xPos < _threshold)
      xOffset = -(_threshold - xPos) / wW;
    else if (xPos > wW - _threshold)
      xOffset = (xPos - (wW - _threshold)) / wW;
    float yOffset = 0.f;
    if (yPos < _threshold)
      yOffset = -(_threshold - yPos) / wH;
    else if (yPos > wH - _threshold)
      yOffset = (yPos - (wH - _threshold)) / wH;

    xOffset *= _moveSpeed;
    yOffset *= _moveSpeed;

    _eye.x += xOffset;
    _eye.z += yOffset;
  }
}

void CameraRTS::mouseNotify(int button, int action, int mods) {}

void CameraRTS::keyNotify(int key, int scancode, int action, int mods) {
  if (action == GLFW_PRESS && key == GLFW_KEY_F1) {
    _attached = true;
  }
}

void CameraRTS::charNotify(unsigned int code) {}

void CameraRTS::scrollNotify(double xOffset, double yOffset) {}