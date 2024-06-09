#pragma once
#include "Core.h"
#include "DebugVisualization.h"

class InputHandler : public InputSubscriber {
 private:
  bool _cursorEnabled = false;
  std::shared_ptr<Core> _core;

 public:
  InputHandler(std::shared_ptr<Core> core);
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
  std::shared_ptr<DebugVisualization> _debugVisualization;
  std::shared_ptr<PointLight> _pointLightHorizontal;
  std::shared_ptr<DirectionalLight> _directionalLight;

 public:
  Main();
  void update();
  void reset(int width, int height);
  void start();
};