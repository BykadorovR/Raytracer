#include "Window.h"

bool Window::getResized() { return _resized; }

void Window::setResized(bool resized) { _resized = resized; }

void* Window::getWindow() { return _window; }

std::tuple<int, int> Window::getResolution() { return _resolution; }

Window::Window(std::tuple<int, int> resolution) { _resolution = resolution; }

void Window::initialize() {
#ifndef __ANDROID__
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  _window = glfwCreateWindow(std::get<0>(_resolution), std::get<1>(_resolution), "Vulkan", nullptr, nullptr);
  glfwSetWindowUserPointer((GLFWwindow*)(_window), this);
  glfwSetFramebufferSizeCallback((GLFWwindow*)(_window), [](GLFWwindow* window, int width, int height) {
    reinterpret_cast<Window*>(glfwGetWindowUserPointer(window))->setResized(true);
  });
#endif
}

#ifdef __ANDROID__
void Window::setNativeWindow(ANativeWindow* window) { _window = window; }
#endif

Window::~Window() {
#ifndef __ANDROID__
  glfwDestroyWindow((GLFWwindow*)(_window));
  glfwTerminate();
#endif
}