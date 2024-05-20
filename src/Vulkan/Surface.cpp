#include "Surface.h"

#ifdef __ANDROID__
Surface::Surface(ANativeWindow* window, std::shared_ptr<Instance> instance) {
  _instance = instance;
  VkAndroidSurfaceCreateInfoKHR createInfo{.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
                                           .pNext = nullptr,
                                           .flags = 0,
                                           .window = window};

  if (vkCreateAndroidSurfaceKHR(instance->getInstance(), &createInfo, nullptr, &_surface) != VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
  }
}
#else
Surface::Surface(std::shared_ptr<Window> window, std::shared_ptr<Instance> instance) {
  _instance = instance;
  if (glfwCreateWindowSurface(instance->getInstance(), window->getWindow(), nullptr, &_surface) != VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
  }
}
#endif

const VkSurfaceKHR& Surface::getSurface() { return _surface; }

Surface::~Surface() { vkDestroySurfaceKHR(_instance->getInstance(), _surface, nullptr); }