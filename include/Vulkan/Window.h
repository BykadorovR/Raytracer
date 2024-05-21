#pragma once
#ifdef __ANDROID__
#include <VulkanWrapper.h>
#else
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif
#include "tuple"
#include <memory>

class Window {
 private:
  void* _window;
  std::tuple<int, int> _resolution;
  bool _resized = false;

 public:
#ifdef __ANDROID__
  Window(ANativeWindow* window, std::tuple<int, int> resolution);
#else
  Window(std::tuple<int, int> resolution);
#endif
  void* getWindow();
  bool getResized();
  void setResized(bool resized);
  std::tuple<int, int> getResolution();
  ~Window();
};