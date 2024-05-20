#ifndef __ANDROID__
#include "Window.h"

Window::Window(std::tuple<int, int> resolution) {
  _resolution = resolution;
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  _window = glfwCreateWindow(std::get<0>(resolution), std::get<1>(resolution), "Vulkan", nullptr, nullptr);
  glfwSetWindowUserPointer(_window, this);
  glfwSetFramebufferSizeCallback(_window, [](GLFWwindow* window, int width, int height) {
    reinterpret_cast<Window*>(glfwGetWindowUserPointer(window))->setResized(true);
  });
}

bool Window::getResized() { return _resized; }

void Window::setResized(bool resized) { _resized = resized; }

GLFWwindow* Window::getWindow() { return _window; }

Window::~Window() {
  glfwDestroyWindow(_window);
  glfwTerminate();
}
#endif