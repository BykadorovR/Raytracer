#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "Graphic/Camera.h"
#include "glm/gtc/matrix_transform.hpp"

CameraDirectionalLight::CameraDirectionalLight() {
  _eye = glm::vec3(0.f, 15.f, 0.f);
  _direction = glm::vec3(0.f, -1.f, 0.f);
  _up = glm::vec3(0.f, 0.f, -1.f);

  _rect = {-20.f, 20.f, -20.f, 20.f};
  _near = 0.1f;
  _far = 40.f;
}

void CameraDirectionalLight::setPosition(glm::vec3 position) { _eye = position; }

void CameraDirectionalLight::setArea(std::array<float, 4> rect, float near, float far) {
  _rect = rect;
  _near = near;
  _far = far;
}

glm::vec3 CameraDirectionalLight::getPosition() { return _eye; }

glm::mat4 CameraDirectionalLight::getView() { return glm::lookAt(_eye, _eye + _direction, _up); }

glm::mat4 CameraDirectionalLight::getProjection() {
  return glm::ortho(_rect[0], _rect[1], _rect[2], _rect[3], _near, _far);
}

CameraPointLight::CameraPointLight() {
  _eye = glm::vec3(0.f, 15.f, 0.f);
  // up is inverted for X and Z because of some specific cubemap Y coordinate stuff
  _direction = {glm::vec3(1.0, 0.0, 0.0),  glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0),
                glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, 1.0),  glm::vec3(0.0, 0.0, -1.0)};
  _up = {glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, 1.0),
         glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, -1.0, 0.0)};

  _near = 0.1f;
  _far = 100.f;
}

void CameraPointLight::setPosition(glm::vec3 position) { _eye = position; }

void CameraPointLight::setArea(float near, float far) {
  _near = near;
  _far = far;
}

float CameraPointLight::getFar() { return _far; }

glm::vec3 CameraPointLight::getPosition() { return _eye; }

glm::mat4 CameraPointLight::getView(int face) {
  auto viewMatrix = glm::lookAt(_eye, _eye + _direction[face], _up[face]);
  return viewMatrix;
}

glm::mat4 CameraPointLight::getProjection() {
  float aspect = 1.f;
  return glm::perspective(glm::radians(90.f), aspect, 0.1f, _far);
}

Camera::Camera() {
  _eye = glm::vec3(0.f, 0.f, 3.f);
  _direction = glm::vec3(0.f, 0.f, -1.f);
  _up = glm::vec3(0.f, 1.f, 0.f);
  _far = 100.f;
  _near = 0.1f;
}

void Camera::setViewParameters(glm::vec3 eye, glm::vec3 direction, glm::vec3 up) {
  _eye = eye;
  _direction = direction;
  _up = up;
}

glm::mat4 Camera::getView() { return glm::lookAt(_eye, _eye + _direction, _up); }

glm::vec3 Camera::getEye() { return _eye; }

glm::vec3 Camera::getDirection() { return _direction; }

glm::vec3 Camera::getUp() { return _up; }

float Camera::getFar() { return _far; }

float Camera::getNear() { return _near; }

void CameraOrtho::setProjectionParameters(std::array<float, 4> rect, float near, float far) {
  _rect = rect;
  _near = near;
  _far = far;
}

glm::mat4 CameraOrtho::getProjection() {
  auto projection = glm::ortho(_rect[0], _rect[1], _rect[2], _rect[3], _near, _far);
  return projection;
}

CameraFly::CameraFly(std::shared_ptr<State> state) : Camera() {
  _state = state;
  _once = false;
  _xLast = 0.f;
  _yLast = 0.f;
  _yaw = -90.f;
  _pitch = 0.f;
  _roll = 0.f;
  _fov = 60.f;
  _direction.x = cos(glm::radians(_yaw)) * cos(glm::radians(_pitch));
  _direction.y = sin(glm::radians(_pitch));
  _direction.z = sin(glm::radians(_yaw)) * cos(glm::radians(_pitch));
  _direction = glm::normalize(_direction);
  _aspect = (float)std::get<0>(_state->getSettings()->getResolution()) /
            (float)std::get<1>(_state->getSettings()->getResolution());
}

void CameraFly::setAspect(float aspect) { _aspect = aspect; }

glm::vec3 CameraFly::getAngles() { return glm::vec3(_yaw, _pitch, _roll); }

void CameraFly::setAngles(float yaw, float pitch, float roll) {
  _yaw = yaw;
  _pitch = pitch;
  _roll = roll;
}

void CameraFly::setSpeed(float rotate, float translate) {
  _sensitivity = rotate;
  _moveSpeed = translate;
}

void CameraFly::setViewParameters(glm::vec3 eye, glm::vec3 direction, glm::vec3 up) {
  Camera::setViewParameters(eye, direction, up);
}

void CameraFly::setProjectionParameters(float fov, float near, float far) {
  _fov = fov;
  _near = near;
  _far = far;
}

glm::mat4 CameraFly::getProjection() {
  auto projection = glm::perspective(glm::radians(_fov), _aspect, _near, _far);
  return projection;
}

float CameraFly::getFOV() { return _fov; }

void CameraFly::cursorNotify(float xPos, float yPos) {
#ifndef __ANDROID__
  if (glfwGetInputMode((GLFWwindow*)(_state->getWindow()->getWindow()), GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
    _once = false;
    return;
  }
#endif

  if (_once == false) {
    _xLast = xPos;
    _yLast = yPos;
    _once = true;
  }

  float xOffset = xPos - _xLast;
  float yOffset = _yLast - yPos;
  _xLast = xPos;
  _yLast = yPos;

  xOffset *= _sensitivity;
  yOffset *= _sensitivity;

  _yaw += xOffset;
  _pitch += yOffset;

  if (_pitch > 89.0f) _pitch = 89.0f;
  if (_pitch < -89.0f) _pitch = -89.0f;

  _direction.x = cos(glm::radians(_yaw)) * cos(glm::radians(_pitch));
  _direction.y = sin(glm::radians(_pitch));
  _direction.z = sin(glm::radians(_yaw)) * cos(glm::radians(_pitch));
  _direction = glm::normalize(_direction);
}

void CameraFly::mouseNotify(int button, int action, int mods) {
#ifdef __ANDROID__
  if (action == 0) _once = false;
#endif
}

void CameraFly::keyNotify(int key, int scancode, int action, int mods) {
#ifdef __ANDROID__
  _keyStatus[key] = true;

  if (action == 0) {
    _keyStatus[key] = false;
  }

  if ((action == 1 && key == 87) || _keyStatus[87]) {
    _eye += _moveSpeed * _direction;
  }
#else
  _keyStatus[key] = true;

  if (action == GLFW_RELEASE) {
    _keyStatus[key] = false;
  }

  if ((action == GLFW_PRESS && key == GLFW_KEY_W) || _keyStatus[GLFW_KEY_W]) {
    _eye += _moveSpeed * _direction;
  }

  if ((action == GLFW_PRESS && key == GLFW_KEY_S) || _keyStatus[GLFW_KEY_S]) {
    _eye -= _moveSpeed * _direction;
  }

  if ((action == GLFW_PRESS && key == GLFW_KEY_A) || _keyStatus[GLFW_KEY_A]) {
    _eye -= _moveSpeed * glm::normalize(glm::cross(_direction, _up));
  }

  if ((action == GLFW_PRESS && key == GLFW_KEY_D) || _keyStatus[GLFW_KEY_D]) {
    _eye += _moveSpeed * glm::normalize(glm::cross(_direction, _up));
  }

  if ((action == GLFW_PRESS && key == GLFW_KEY_SPACE) || _keyStatus[GLFW_KEY_SPACE]) {
    _eye += _moveSpeed * _up;
  }
#endif
}

void CameraFly::charNotify(unsigned int code) {}

void CameraFly::scrollNotify(double xOffset, double yOffset) {}