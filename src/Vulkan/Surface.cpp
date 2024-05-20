#include "Surface.h"

Surface::Surface(std::shared_ptr<Window> window, std::shared_ptr<Instance> instance) {
  _instance = instance;
#ifdef __ANDROID__
  VkAndroidSurfaceCreateInfoKHR createInfo{.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
                                           .pNext = nullptr,
                                           .flags = 0,
                                           .window = std::any_cast<ANativeWindow*>(window->getWindow())};

  if (vkCreateAndroidSurfaceKHR(instance->getInstance(), &createInfo, nullptr, &_surface) != VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
  }
#else
  if (glfwCreateWindowSurface(instance->getInstance(), std::any_cast<GLFWwindow*>(window->getWindow()), nullptr,
                              &_surface) != VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
  }
#endif
}

const VkSurfaceKHR& Surface::getSurface() { return _surface; }

Surface::~Surface() { vkDestroySurfaceKHR(_instance->getInstance(), _surface, nullptr); }