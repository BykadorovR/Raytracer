#pragma once
#ifndef __ANDROID__
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif
#include "tuple"
#include <any>
#include "Settings.h"
#include <memory>

class Window {
 private:
  std::any _window;
  std::tuple<int, int> _resolution;
  bool _resized = false;

 public:
  Window(std::shared_ptr<Settings> settings);
  std::any getWindow();
  bool getResized();
  void setResized(bool resized);
  std::tuple<int, int> getResolution();
  ~Window();
};