#pragma once
#include "Window.h"
#include "tuple"
#include <vector>
#include <memory>
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