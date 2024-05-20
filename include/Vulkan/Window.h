#pragma once
#ifndef __ANDROID__
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "tuple"

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

  ~Window();
};
#endif