#pragma once
#include "Core.h"
#include "DebugVisualization.h"

class InputHandler : public InputSubscriber {
 private:
  bool _cursorEnabled = false;

 public:
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
  std::shared_ptr<DebugVisualization> _debugVisualization;
  std::shared_ptr<PointLight> _pointLightHorizontal;
  std::shared_ptr<DirectionalLight> _directionalLight;

 public:
  Main();
  void update();
  void reset(int width, int height);
  void start();
};