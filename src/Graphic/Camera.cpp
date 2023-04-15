#include "Camera.h"
#include "glm/gtc/matrix_transform.hpp"

Camera::Camera(std::shared_ptr<Settings> settings) {
  _settings = settings;
  _viewParams = std::make_shared<ViewParameters>();
  _viewParams->eye = glm::vec3(0.f, 0.f, 3.f);
  _viewParams->direction = glm::vec3(0.f, 0.f, -3.f);
  _viewParams->up = glm::vec3(0.f, 1.f, 0.f);
  _projectionParams = std::make_shared<ProjectionParameters>();
  _projectionParams->far = 1000.f;
  _projectionParams->near = 0.1f;
  _projectionParams->fov = 60.f;
}

void Camera::setViewParameters(std::shared_ptr<ViewParameters> viewParams) { _viewParams = viewParams; }

void Camera::setProjectionParameters(std::shared_ptr<ProjectionParameters> projectionParams) {
  _projectionParams = projectionParams;
}

glm::mat4 Camera::getView() {
  return glm::lookAt(_viewParams->eye, _viewParams->eye + _viewParams->direction, _viewParams->up);
}

glm::mat4 Camera::getProjection() {
  auto projection = glm::perspective(
      glm::radians(_projectionParams->fov),
      (float)std::get<0>(_settings->getResolution()) / (float)std::get<1>(_settings->getResolution()),
      _projectionParams->near, _projectionParams->far);
  return projection;
}

std::shared_ptr<ViewParameters> Camera::getViewParameters() { return _viewParams; }

CameraFly::CameraFly(std::shared_ptr<Settings> settings) : Camera(settings) {
  _once = false;
  _xLast = 0.f;
  _yLast = 0.f;
  _yaw = -90.f;
  _pitch = 0.f;
  _roll = 0.f;
  _viewParams->direction.x = cos(glm::radians(_yaw)) * cos(glm::radians(_pitch));
  _viewParams->direction.y = sin(glm::radians(_pitch));
  _viewParams->direction.z = sin(glm::radians(_yaw)) * cos(glm::radians(_pitch));
  _viewParams->direction = glm::normalize(_viewParams->direction);
}

void CameraFly::cursorNotify(float xPos, float yPos) {
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

  _viewParams->direction.x = cos(glm::radians(_yaw)) * cos(glm::radians(_pitch));
  _viewParams->direction.y = sin(glm::radians(_pitch));
  _viewParams->direction.z = sin(glm::radians(_yaw)) * cos(glm::radians(_pitch));
  _viewParams->direction = glm::normalize(_viewParams->direction);
}

void CameraFly::mouseNotify(int button, int action, int mods) {}

void CameraFly::keyNotify(int key, int action, int mods) {
  _keyStatus[key] = true;

  if (action == GLFW_RELEASE) {
    _keyStatus[key] = false;
  }

  if ((action == GLFW_PRESS && key == GLFW_KEY_W) || _keyStatus[GLFW_KEY_W]) {
    _viewParams->eye += _sensitivity * _viewParams->direction;
  }

  if ((action == GLFW_PRESS && key == GLFW_KEY_S) || _keyStatus[GLFW_KEY_S]) {
    _viewParams->eye -= _sensitivity * _viewParams->direction;
  }

  if ((action == GLFW_PRESS && key == GLFW_KEY_A) || _keyStatus[GLFW_KEY_A]) {
    _viewParams->eye -= _sensitivity * glm::normalize(glm::cross(_viewParams->direction, _viewParams->up));
  }

  if ((action == GLFW_PRESS && key == GLFW_KEY_D) || _keyStatus[GLFW_KEY_D]) {
    _viewParams->eye += _sensitivity * glm::normalize(glm::cross(_viewParams->direction, _viewParams->up));
  }

  if ((action == GLFW_PRESS && key == GLFW_KEY_SPACE) || _keyStatus[GLFW_KEY_SPACE]) {
    _viewParams->eye += _sensitivity * _viewParams->up;
  }
}