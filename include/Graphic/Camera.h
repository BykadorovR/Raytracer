#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Settings.h"
#include "Window.h"
#include "Input.h"
#include <memory>
#include <map>
#include <array>
#undef near
#undef far

class Camera {
 protected:
  // projection
  float _near;
  float _far;
  // camera
  glm::vec3 _eye;
  glm::vec3 _direction;
  glm::vec3 _up;

 public:
  Camera();
  glm::mat4 getView();
  virtual glm::mat4 getProjection() = 0;
  void setViewParameters(glm::vec3 eye, glm::vec3 direction, glm::vec3 up);
  glm::vec3 getEye();
  glm::vec3 getDirection();
  glm::vec3 getUp();
  float getFar();
  float getNear();
};

class CameraOrtho : public Camera {
 private:
  std::array<float, 4> _rect;

 public:
  void setProjectionParameters(std::array<float, 4> rect, float near, float far);
  glm::mat4 getProjection() override;
};

class CameraFly : public Camera, public InputSubscriber {
 private:
  float _fov;
  float _yaw;
  float _pitch;
  float _roll;
  float _xLast;
  float _yLast;
  std::map<int, bool> _keyStatus;
  std::shared_ptr<Settings> _settings;
  bool _once;
  float _sensitivity = 0.1f;

 public:
  CameraFly(std::shared_ptr<Settings> settings);
  void setProjectionParameters(float fov, float near, float far);
  glm::mat4 getProjection() override;
  float getFOV();
  void cursorNotify(GLFWwindow* window, float xPos, float yPos);
  void mouseNotify(GLFWwindow* window, int button, int action, int mods);
  void keyNotify(GLFWwindow* window, int key, int action, int mods);
};