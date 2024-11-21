#include "Graphic/CameraRTS.h"

CameraRTS::CameraRTS(std::shared_ptr<Drawable> object, std::shared_ptr<EngineState> engineState) {
  _object = object;
  _engineState = engineState;
  auto model = _object->getModel() * _object->getOrigin();
  _eye = glm::vec3(model[3][0], model[3][1], model[3][2]) + _shift;
  _direction = glm::normalize(glm::vec3(model[3][0], model[3][1], model[3][2]) - _eye);
  _up = glm::vec3(0.f, 0.f, -1.f);
  _aspect = ((float)std::get<0>(_engineState->getSettings()->getResolution()) /
             (float)std::get<1>(_engineState->getSettings()->getResolution()));
}

void CameraRTS::update() {
  _eye.x += _offset.first;
  _eye.z += _offset.second;
}

void CameraRTS::setShift(glm::vec3 shift) { _shift = shift; }

void CameraRTS::setThreshold(int threshold) { _threshold = threshold; }

glm::mat4 CameraRTS::getView() {
  if (_attached) {
    auto model = _object->getModel() * _object->getOrigin();
    _eye = glm::vec3(model[3][0], model[3][1], model[3][2]) + _shift;
    _direction = glm::normalize(glm::vec3(model[3][0], model[3][1], model[3][2]) - _eye);
  }
  return glm::lookAt(_eye, _eye + _direction, _up);
}

void CameraRTS::cursorNotify(float xPos, float yPos) {
  auto [wW, wH] = _engineState->getSettings()->getResolution();
  _offset = {0, 0};
  if (xPos < _threshold || yPos < _threshold || xPos > wW - _threshold || yPos > wH - _threshold) {
    _attached = false;
    // need to move camera only if cursor is near the end of the window
    if (xPos < _threshold)
      _offset.first = -(_threshold - xPos) / wW;
    else if (xPos > wW - _threshold)
      _offset.first = (xPos - (wW - _threshold)) / wW;
    if (yPos < _threshold)
      _offset.second = -(_threshold - yPos) / wH;
    else if (yPos > wH - _threshold)
      _offset.second = (yPos - (wH - _threshold)) / wH;

    _offset.first *= _moveSpeed;
    _offset.second *= _moveSpeed;
  }
}

void CameraRTS::mouseNotify(int button, int action, int mods) {}

void CameraRTS::keyNotify(int key, int scancode, int action, int mods) {
#ifndef __ANDROID__
  if (action == GLFW_PRESS && key == GLFW_KEY_F1) {
    _attached = true;
  }
#endif
}

void CameraRTS::charNotify(unsigned int code) {}

void CameraRTS::scrollNotify(double xOffset, double yOffset) {}