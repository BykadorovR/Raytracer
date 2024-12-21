#include "Graphic/CameraRTS.h"

CameraRTS::CameraRTS(std::shared_ptr<Drawable> object, std::shared_ptr<EngineState> engineState)
    : CameraPerspective(engineState) {
  _object = object;
  _engineState = engineState;
  _eye = _object->getTranslate() + _shift;
  _direction = glm::normalize(_object->getTranslate() - _eye);
  _up = glm::vec3(0.f, 0.f, -1.f);
  _aspect = (static_cast<float>(std::get<0>(_engineState->getSettings()->getResolution())) /
             static_cast<float>(std::get<1>(_engineState->getSettings()->getResolution())));
}

void CameraRTS::update() {
  _eye.x += _offset.first;
  _eye.z += _offset.second;
}

void CameraRTS::setShift(glm::vec3 shift) { _shift = shift; }

void CameraRTS::setThreshold(int threshold) { _threshold = threshold; }

void CameraRTS::cursorNotify(float xPos, float yPos) {
  auto [wW, wH] = _engineState->getSettings()->getResolution();
  _offset = {0, 0};
  if (xPos < _threshold || yPos < _threshold || xPos > wW - _threshold || yPos > wH - _threshold) {
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
    _zoomPoint.reset();
  }
}

void CameraRTS::mouseNotify(int button, int action, int mods) {}

void CameraRTS::keyNotify(int key, int scancode, int action, int mods) {
#ifndef __ANDROID__
  if (action == GLFW_PRESS && key == GLFW_KEY_F1) {
    _eye = _object->getTranslate() + _shift;
    _direction = glm::normalize(_object->getTranslate() - _eye);
    _zoomPoint.reset();
  }
#endif
}

void CameraRTS::charNotify(unsigned int code) {}

void CameraRTS::scrollNotify(double xOffset, double yOffset) {
  if (_zoomPoint.has_value() == false) {
    _zoomPoint = {_eye.x - _shift.x, _object->getTranslate().y, _eye.z - _shift.z};
  }
  auto eye = _eye;
  eye.y -= yOffset;
  auto direction = glm::normalize(_zoomPoint.value() - eye);

  if (direction.y < 0) {
    _direction = direction;
    _eye = eye;
  }
}