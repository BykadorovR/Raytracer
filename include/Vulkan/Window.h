#pragma once
#ifndef __ANDROID__
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif
#include "tuple"
#include <any>
#include <memory>

class Window {
 private:
  std::any _window;
  std::tuple<int, int> _resolution;
  bool _resized = false;

 public:
#ifdef __ANDROID__
  Window(ANativeWindow* window, std::tuple<int, int> resolution);
#else
  Window(std::tuple<int, int> resolution);
#endif
  std::any getWindow();
  bool getResized();
  void setResized(bool resized);
  std::tuple<int, int> getResolution();
  ~Window();
};