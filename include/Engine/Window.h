#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "tuple"
#include <vector>
#include <memory>

class Window {
 private:
  GLFWwindow* _window;
  std::tuple<int, int> _resolution;
  bool _resolutionChanged;

 public:
  Window(std::tuple<int, int> resolution);
  bool isResolutionChanged();
  void setResolutionChanged(bool state);
  GLFWwindow* getWindow();
  std::vector<const char*> getExtensions();

  ~Window();
};