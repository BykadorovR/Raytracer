#include "DebugVisualization.h"

DebugVisualization::DebugVisualization(std::shared_ptr<Model3DManager> modelManager,
                                       std::shared_ptr<Camera> camera,
                                       std::shared_ptr<GUI> gui,
                                       std::shared_ptr<Window> window) {
  _camera = camera;
  _window = window;
  _modelManager = modelManager;
  _gui = gui;
}

void DebugVisualization::setLights(std::shared_ptr<LightManager> lightManager) {
  _lightManager = lightManager;
  for (auto light : lightManager->getLights()) {
    auto model = _modelManager->createModelGLTF("../data/Box/BoxTextured.gltf");
    _modelManager->registerModelGLTF(model);
    _lightModels.push_back(model);
  }
}

void DebugVisualization::draw() {
  if (_lightManager) {
    std::map<std::string, bool*> toggle;
    toggle["Lights"] = &_showLights;
    _gui->addCheckbox("Debug", {20, 100}, {100, 60}, toggle);
    if (_showLights) {
      for (int i = 0; i < _lightManager->getLights().size(); i++) {
        if (_registerLights) _modelManager->registerModelGLTF(_lightModels[i]);
        auto model = glm::translate(glm::mat4(1.f), _lightManager->getLights()[i]->getPosition());
        _lightModels[i]->setModel(model);
        _lightModels[i]->setProjection(_camera->getProjection());
        _lightModels[i]->setView(_camera->getView());
      }
      _registerLights = false;
    } else {
      if (_registerLights == false) {
        _registerLights = true;
        for (int i = 0; i < _lightManager->getLights().size(); i++) {
          _modelManager->unregisterModelGLTF(_lightModels[i]);
        }
      }
    }
  }
}

void DebugVisualization::cursorNotify(float xPos, float yPos) {}

void DebugVisualization::mouseNotify(int button, int action, int mods) {}

void DebugVisualization::keyNotify(int key, int action, int mods) {
  if ((action == GLFW_RELEASE && key == GLFW_KEY_C)) {
    if (_cursorEnabled) {
      glfwSetInputMode(_window->getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      _cursorEnabled = false;
    } else {
      glfwSetInputMode(_window->getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      _cursorEnabled = true;
    }
  }
}