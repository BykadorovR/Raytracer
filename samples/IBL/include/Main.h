#pragma once
#include <windows.h>
#undef near
#undef far
import Core;
import Light;
import Shape3D;
import Camera;

class InputHandler : public VulkanEngine::InputSubscriber {
 private:
  bool _cursorEnabled = false;

 public:
  void cursorNotify(GLFWwindow* window, float xPos, float yPos) override;
  void mouseNotify(GLFWwindow* window, int button, int action, int mods) override;
  void keyNotify(GLFWwindow* window, int key, int scancode, int action, int mods) override;
  void charNotify(GLFWwindow* window, unsigned int code) override;
  void scrollNotify(GLFWwindow* window, double xOffset, double yOffset) override;
};

class Main {
 private:
  std::shared_ptr<VulkanEngine::Core> _core;
  std::shared_ptr<VulkanEngine::CameraFly> _camera;
  std::shared_ptr<InputHandler> _inputHandler;

  std::shared_ptr<VulkanEngine::Shape3D> _cubeTextured, _cubeTexturedWireframe;
  std::shared_ptr<VulkanEngine::PointLight> _pointLightVertical, _pointLightHorizontal;
  std::shared_ptr<VulkanEngine::DirectionalLight> _directionalLight;
  std::shared_ptr<VulkanEngine::Shape3D> _cubeColoredLightVertical, _cubeColoredLightHorizontal;
  float _directionalValue = 0.5f, _pointVerticalValue = 1.f, _pointHorizontalValue = 10.f;

 public:
  Main();
  void update();
  void reset(int width, int height);
  void start();
};