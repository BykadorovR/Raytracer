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

 public:
  Window(std::tuple<int, int> resolution);
  GLFWwindow* getWindow();
  std::vector<const char*> getExtensions();

  ~Window();
};