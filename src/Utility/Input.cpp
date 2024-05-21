#include "Input.h"
#include <memory>

Input::Input(std::shared_ptr<Window> window) {
#ifndef __ANDROID__
  _window = window;
  glfwSetWindowUserPointer(std::any_cast<GLFWwindow*>(window->getWindow()), this);
  glfwSetInputMode(std::any_cast<GLFWwindow*>(window->getWindow()), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(std::any_cast<GLFWwindow*>(window->getWindow()),
                           [](GLFWwindow* window, double xpos, double ypos) {
                             reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->cursorHandler(xpos, ypos);
                           });
  glfwSetMouseButtonCallback(
      std::any_cast<GLFWwindow*>(window->getWindow()), [](GLFWwindow* window, int button, int action, int mods) {
        reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->mouseHandler(button, action, mods);
      });
  glfwSetCharCallback(std::any_cast<GLFWwindow*>(window->getWindow()), [](GLFWwindow* window, unsigned int code) {
    reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->charHandler(code);
  });
  glfwSetKeyCallback(std::any_cast<GLFWwindow*>(window->getWindow()), [](GLFWwindow* window, int key, int scancode,
                                                                         int action, int mods) {
    reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->keyHandler(key, scancode, action, mods);
  });
  glfwSetScrollCallback(std::any_cast<GLFWwindow*>(window->getWindow()),
                        [](GLFWwindow* window, double xOffset, double yOffset) {
                          reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->scrollHandler(xOffset, yOffset);
                        });
#endif
}

void Input::keyHandler(int key, int scancode, int action, int mods) {
  for (auto& sub : _subscribers) {
    sub->keyNotify(key, scancode, action, mods);
  }
}

void Input::charHandler(unsigned int code) {
  for (auto& sub : _subscribers) {
    sub->charNotify(code);
  }
}

void Input::scrollHandler(double xOffset, double yOffset) {
  for (auto& sub : _subscribers) {
    sub->scrollNotify(xOffset, yOffset);
  }
}

void Input::cursorHandler(double xpos, double ypos) {
  for (auto& sub : _subscribers) {
    sub->cursorNotify(xpos, ypos);
  }
}

void Input::mouseHandler(int button, int action, int mods) {
  for (auto& sub : _subscribers) {
    sub->mouseNotify(button, action, mods);
  }
}

void Input::subscribe(std::shared_ptr<InputSubscriber> sub) { _subscribers.push_back(sub); }

void Input::showCursor(bool show) {
  if (show)
    glfwSetInputMode(std::any_cast<GLFWwindow*>(_window->getWindow()), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  else
    glfwSetInputMode(std::any_cast<GLFWwindow*>(_window->getWindow()), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}