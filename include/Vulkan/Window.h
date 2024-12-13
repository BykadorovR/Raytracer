#pragma once
#include "volk.h"  //defines VK_NO_PROTOTYPES & defines VK_KHR_win32_surface/VK_KHR_android_surface (inside vulkan.h and vulkan_android.h)
#ifndef __ANDROID__
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>  //vulkan.h is included inside glfw so VK_NO_PROTOTYPES helps
#endif
#include "tuple"
#include <memory>
#undef min
#undef max

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
  ~Window();
};