#pragma once
#include "ModelManager.h"
#include "GUI.h"
#include "LightManager.h"

class DebugVisualization : public InputSubscriber {
 private:
  std::vector<std::shared_ptr<ModelGLTF>> _lightModels;
  std::shared_ptr<LightManager> _lightManager = nullptr;
  bool _showLights = true;
  bool _registerLights = false;
  std::shared_ptr<Camera> _camera;
  std::shared_ptr<Model3DManager> _modelManager;
  std::shared_ptr<GUI> _gui;

  bool _cursorEnabled = false;

 public:
  DebugVisualization(std::shared_ptr<Model3DManager> modelManager,
                     std::shared_ptr<Camera> camera,
                     std::shared_ptr<GUI> gui);
  void setLights(std::shared_ptr<LightManager> lightManager);
  void draw();

  void cursorNotify(GLFWwindow* window, float xPos, float yPos);
  void mouseNotify(GLFWwindow* window, int button, int action, int mods);
  void keyNotify(GLFWwindow* window, int key, int action, int mods);
};