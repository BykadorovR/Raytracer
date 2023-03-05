#pragma once
#include "glm/glm.hpp"
#include "Settings.h"
#include "Input.h"
#include <memory>
#include <map>

struct ViewParameters {
  glm::vec3 eye;
  glm::vec3 direction;
  glm::vec3 up;
};

struct ProjectionParameters {
  float fov;
  float near;
  float far;
};

class Camera {
 protected:
  std::shared_ptr<ViewParameters> _viewParams;
  std::shared_ptr<ProjectionParameters> _projectionParams;
  std::shared_ptr<Settings> _settings;

 public:
  Camera(std::shared_ptr<Settings> settings);
  void setViewParameters(std::shared_ptr<ViewParameters> viewParams);
  void setProjectionParameters(std::shared_ptr<ProjectionParameters> projectionParams);
  glm::mat4 getView();
  glm::mat4 getProjection();
};

class CameraFly : public Camera, public InputSubscriber {
 private:
  float _yaw;
  float _pitch;
  float _roll;
  float _xLast;
  float _yLast;
  std::map<int, bool> _keyStatus;

  bool _once;
  float _sensitivity = 0.1f;

 public:
  CameraFly(std::shared_ptr<Settings> settings);
  void cursorNotify(float xPos, float yPos);
  void mouseNotify(int button, int action, int mods);
  void keyNotify(int key, int action, int mods);
};