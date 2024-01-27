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
  bool _resized = false;

 public:
  Window(std::tuple<int, int> resolution);
  GLFWwindow* getWindow();
  bool getResized();
  void setResized(bool resized);
  std::vector<const char*> getExtensions();

  ~Window();
};