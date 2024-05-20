#pragma once
#include "Core.h"
#include "Shape3D.h"
#include "Terrain.h"

class InputHandler : public InputSubscriber {
 private:
  bool _cursorEnabled = false;
  std::function<void(glm::vec3)> _callback;

 public:
  void setMoveCallback(std::function<void(glm::vec3)> callback);
  void cursorNotify(std::any window, float xPos, float yPos) override;
  void mouseNotify(std::any window, int button, int action, int mods) override;
  void keyNotify(std::any window, int key, int scancode, int action, int mods) override;
  void charNotify(std::any window, unsigned int code) override;
  void scrollNotify(std::any window, double xOffset, double yOffset) override;
};

class Main {
 private:
  std::shared_ptr<Core> _core;
  std::shared_ptr<CameraFly> _camera;
  std::shared_ptr<InputHandler> _inputHandler;

  std::shared_ptr<PointLight> _pointLightVertical, _pointLightHorizontal;
  std::shared_ptr<DirectionalLight> _directionalLight;
  std::shared_ptr<Shape3D> _cubeColoredLightVertical, _cubeColoredLightHorizontal, _cubePlayer;
  std::shared_ptr<Terrain> _terrainColor, _terrainPhong, _terrainPBR;
  bool _showLoD = false, _showWireframe = false, _showNormals = false, _showPatches = false;

 public:
  Main();
  void update();
  void reset(int width, int height);
  void start();
};