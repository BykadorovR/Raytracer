#pragma once
#include "Core.h"
#include "Line.h"
#include "Sprite.h"
#include "Model.h"
#include "Shape3D.h"

class DebugVisualization : public InputSubscriber {
 private:
  std::shared_ptr<Core> _core;
  std::vector<std::shared_ptr<Line>> _lineFrustum;
  std::shared_ptr<Sprite> _farPlaneCW, _farPlaneCCW;
  std::vector<std::shared_ptr<Model3D>> _pointLightModels, _directionalLightModels;
  std::vector<std::shared_ptr<Shape3D>> _spheres;
  std::shared_ptr<Sprite> _spriteShadow;

  bool _showLights = true;
  bool _registerLights = false;
  std::shared_ptr<Camera> _camera;
  bool _cursorEnabled = false;
  bool _showDepth = true;
  bool _showNormals = false;
  bool _showWireframe = false;
  bool _initializedDepth = false;
  int _lightSpheresIndex = -1;
  int _shadowMapIndex = 0;
  bool _enableSpheres = false;
  std::vector<std::string> _attenuationKeys;
  std::vector<std::string> _shadowKeys;
  bool _frustumDraw = false;
  bool _showPlanes = false;
  bool _planesRegistered = false;
  float _near, _far;
  float _R, _G, _B;
  glm::vec3 _eyeSave = {0, 0, 3}, _dirSave = {0, 0, -1}, _upSave = {0, 1, 0}, _angles = {-90, 0, 0};
  float _directionalValue, _pointValue;
  std::shared_ptr<MaterialColor> _materialShadow;
  void _drawFrustum();
  void _drawShadowMaps();

 public:
  DebugVisualization(std::shared_ptr<Camera> camera, std::shared_ptr<Core> core);
  void update();

  void cursorNotify(GLFWwindow* window, float xPos, float yPos) override;
  void mouseNotify(GLFWwindow* window, int button, int action, int mods) override;
  void keyNotify(GLFWwindow* window, int key, int scancode, int action, int mods) override;
  void charNotify(GLFWwindow* window, unsigned int code) override;
  void scrollNotify(GLFWwindow* window, double xOffset, double yOffset) override;
};