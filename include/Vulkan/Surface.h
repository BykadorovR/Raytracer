#pragma once
#include "Vulkan/Window.h"
#include "Vulkan/Instance.h"
#include "tuple"
#include <vector>
#include <memory>

class Surface {
 private:
  VkSurfaceKHR _surface;
  std::shared_ptr<Instance> _instance;

 public:
  Surface(std::shared_ptr<Window> window, std::shared_ptr<Instance> instance);
  const VkSurfaceKHR& getSurface();
  ~Surface();
};