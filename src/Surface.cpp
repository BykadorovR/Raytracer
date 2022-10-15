#include "Surface.h"

Surface::Surface(std::shared_ptr<Window> window, std::shared_ptr<Instance> instance) {
  _instance = instance;
  if (glfwCreateWindowSurface(instance->getInstance(), window->getWindow(), nullptr, &_surface) != VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
  }
}

const VkSurfaceKHR& Surface::getSurface() { return _surface; }

Surface::~Surface() { vkDestroySurfaceKHR(_instance->getInstance(), _surface, nullptr); }