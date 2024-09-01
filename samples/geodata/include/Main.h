#pragma once
#include "Engine/Core.h"
#include "Primitive/Shape3D.h"
#include "Primitive/Terrain.h"

class InputHandler : public InputSubscriber {
 private:
  bool _cursorEnabled = false;
  std::function<void(std::optional<glm::vec3>)> _callback;
  std::shared_ptr<Core> _core;

 public:
  InputHandler(std::shared_ptr<Core> core);
  void setMoveCallback(std::function<void(std::optional<glm::vec3>)> callback);
  void cursorNotify(float xPos, float yPos) override;
  void mouseNotify(int button, int action, int mods) override;
  void keyNotify(int key, int scancode, int action, int mods) override;
  void charNotify(unsigned int code) override;
  void scrollNotify(double xOffset, double yOffset) override;
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
  std::shared_ptr<PhysicsManager> _physicsManager;
  std::shared_ptr<TerrainPhysics> _terrainPhysics;
  std::shared_ptr<Shape3DPhysics> _shape3DPhysics;
  std::optional<glm::vec3> _shift;
  bool _showLoD = false, _showWireframe = false, _showNormals = false, _showPatches = false;

 public:
  Main();
  void update();
  void reset(int width, int height);
  void start();
};