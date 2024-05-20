#pragma once
#ifdef __ANDROID__
#include <game-activity/native_app_glue/android_native_app_glue.h>
#else
#include "Window.h"
#endif
#include "tuple"
#include <vector>
#include <memory>
#include "Instance.h"

class Surface {
 private:
  VkSurfaceKHR _surface;
  std::shared_ptr<Instance> _instance;

 public:
#ifdef __ANDROID__
  Surface(ANativeWindow* window, std::shared_ptr<Instance> instance);
#else
  Surface(std::shared_ptr<Window> window, std::shared_ptr<Instance> instance);
#endif
  const VkSurfaceKHR& getSurface();
  ~Surface();
};