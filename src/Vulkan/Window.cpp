#include "Vulkan/Window.h"

bool Window::getResized() { return _resized; }

void Window::setResized(bool resized) { _resized = resized; }

void* Window::getWindow() { return _window; }

Window::Window(std::tuple<int, int> resolution) { _resolution = resolution; }

void Window::setFullScreen(bool fullScreen) {
#ifndef __ANDROID__
  if (fullScreen) {
    auto monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    glfwSetWindowMonitor((GLFWwindow*)_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
  } else {
    glfwSetWindowMonitor((GLFWwindow*)_window, nullptr, 0, 0, std::get<0>(_resolution), std::get<1>(_resolution),
                         GLFW_DONT_CARE);
  }
#endif
}

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