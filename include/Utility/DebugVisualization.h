#pragma once
#include "ModelManager.h"
#include "GUI.h"
#include "LightManager.h"

class DebugVisualization : public InputSubscriber {
 private:
  std::vector<std::shared_ptr<ModelGLTF>> _lightModels;
  std::shared_ptr<LightManager> _lightManager = nullptr;
  std::shared_ptr<Window> _window;
  bool _showLights = true;
  bool _registerLights = false;
  std::shared_ptr<Camera> _camera;
  std::shared_ptr<Model3DManager> _modelManager;
  std::shared_ptr<GUI> _gui;

  bool _cursorEnabled = false;

 public:
  DebugVisualization(std::shared_ptr<Model3DManager> modelManager,
                     std::shared_ptr<Camera> camera,
                     std::shared_ptr<GUI> gui,
                     std::shared_ptr<Window> window);
  void setLights(std::shared_ptr<LightManager> lightManager);
  void draw();

  void cursorNotify(float xPos, float yPos);
  void mouseNotify(int button, int action, int mods);
  void keyNotify(int key, int action, int mods);
};