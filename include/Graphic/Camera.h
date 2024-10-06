#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <map>
#include <array>
#include "Utility/EngineState.h"
#include "Utility/Input.h"
#include "Vulkan/Window.h"
#undef near
#undef far

class CameraDirectionalLight {
 protected:
  // projection
  std::array<float, 4> _rect;
  float _near;
  float _far;
  // camera
  glm::vec3 _eye;
  glm::vec3 _direction;
  glm::vec3 _up;

 public:
  CameraDirectionalLight();
  void setPosition(glm::vec3 position);
  void setArea(std::array<float, 4> rect, float near, float far);
  glm::vec3 getPosition();
  glm::mat4 getView();
  glm::mat4 getProjection();
};

class CameraPointLight {
 private:
  // projection
  float _near;
  float _far;
  // camera
  glm::vec3 _eye;
  std::array<glm::vec3, 6> _direction;
  std::array<glm::vec3, 6> _up;

 public:
  CameraPointLight();
  void setPosition(glm::vec3 position);
  void setArea(float near, float far);
  float getFar();
  glm::vec3 getPosition();
  glm::mat4 getView(int face);
  glm::mat4 getProjection();
};

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
  std::shared_ptr<EngineState> _engineState;
  bool _once;
  float _sensitivity = 0.1f;
  float _moveSpeed = 0.1f;
  float _aspect;

 public:
  CameraFly(std::shared_ptr<EngineState> engineState);
  void setProjectionParameters(float fov, float near, float far);
  void setViewParameters(glm::vec3 eye, glm::vec3 direction, glm::vec3 up);
  void setSpeed(float rotate, float translate);
  glm::mat4 getProjection() override;
  glm::vec3 getAngles();
  void setAngles(float yaw, float pitch, float roll);
  void setAspect(float aspect);
  float getFOV();
  void cursorNotify(float xPos, float yPos) override;
  void mouseNotify(int button, int action, int mods) override;
  void keyNotify(int key, int scancode, int action, int mods) override;
  void charNotify(unsigned int code) override;
  void scrollNotify(double xOffset, double yOffset) override;
};