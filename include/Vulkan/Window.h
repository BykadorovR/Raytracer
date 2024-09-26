#pragma once
#ifdef __ANDROID__
#include <Platform/Android/VulkanWrapper.h>
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
  Window(std::tuple<int, int> resolution);
#ifdef __ANDROID__
  void setNativeWindow(ANativeWindow* window);
#endif
  void initialize();
  void* getWindow();
  bool getResized();
  void setResized(bool resized);
  std::tuple<int, int> getResolution();
  ~Window();
};