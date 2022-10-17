#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "tuple"
#include <vector>
#include <memory>

#include "Window.h"
#include "Instance.h"

class Surface {
 private:
  VkSurfaceKHR _surface;
  std::shared_ptr<Instance> _instance;

 public:
  Surface(std::shared_ptr<Window> window, std::shared_ptr<Instance> instance);
  const VkSurfaceKHR& getSurface();
  ~Surface();
};