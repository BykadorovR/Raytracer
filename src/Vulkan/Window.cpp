#include "Window.h"

Window::Window(std::shared_ptr<Settings> settings) {
  _resolution = settings->getResolution();
#ifdef __ANDROID__
  _window = settings->getAndroidApp()->window;
#else
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  _window = glfwCreateWindow(std::get<0>(_resolution), std::get<1>(_resolution), "Vulkan", nullptr, nullptr);
  glfwSetWindowUserPointer(std::any_cast<GLFWwindow*>(_window), this);
  glfwSetFramebufferSizeCallback(std::any_cast<GLFWwindow*>(_window), [](GLFWwindow* window, int width, int height) {
    reinterpret_cast<Window*>(glfwGetWindowUserPointer(window))->setResized(true);
  });
#endif
}

bool Window::getResized() { return _resized; }

void Window::setResized(bool resized) { _resized = resized; }

std::any Window::getWindow() { return _window; }

std::tuple<int, int> Window::getResolution() { return _resolution; }

Window::~Window() {
#ifndef __ANDROID__
  glfwDestroyWindow(std::any_cast<GLFWwindow*>(_window));
  glfwTerminate();
#endif
}