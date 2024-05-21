#include "Input.h"
#include <memory>

Input::Input(std::shared_ptr<Window> window) {
#ifndef __ANDROID__
  glfwSetWindowUserPointer(std::any_cast<GLFWwindow*>(window->getWindow()), this);
  glfwSetInputMode(std::any_cast<GLFWwindow*>(window->getWindow()), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(
      std::any_cast<GLFWwindow*>(window->getWindow()), [](GLFWwindow* window, double xpos, double ypos) {
        reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->cursorHandler(window, xpos, ypos);
      });
  glfwSetMouseButtonCallback(
      std::any_cast<GLFWwindow*>(window->getWindow()), [](GLFWwindow* window, int button, int action, int mods) {
        reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->mouseHandler(window, button, action, mods);
      });
  glfwSetCharCallback(std::any_cast<GLFWwindow*>(window->getWindow()), [](GLFWwindow* window, unsigned int code) {
    reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->charHandler(window, code);
  });
  glfwSetKeyCallback(std::any_cast<GLFWwindow*>(window->getWindow()), [](GLFWwindow* window, int key, int scancode,
                                                                         int action, int mods) {
    reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->keyHandler(window, key, scancode, action, mods);
  });
  glfwSetScrollCallback(
      std::any_cast<GLFWwindow*>(window->getWindow()), [](GLFWwindow* window, double xOffset, double yOffset) {
        reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->scrollHandler(window, xOffset, yOffset);
      });
#endif
}

void Input::keyHandler(std::any window, int key, int scancode, int action, int mods) {
  for (auto& sub : _subscribers) {
    sub->keyNotify(window, key, scancode, action, mods);
  }
}

void Input::charHandler(std::any window, unsigned int code) {
  for (auto& sub : _subscribers) {
    sub->charNotify(window, code);
  }
}

void Input::scrollHandler(std::any window, double xOffset, double yOffset) {
  for (auto& sub : _subscribers) {
    sub->scrollNotify(window, xOffset, yOffset);
  }
}

void Input::cursorHandler(std::any window, double xpos, double ypos) {
  for (auto& sub : _subscribers) {
    sub->cursorNotify(window, xpos, ypos);
  }
}

void Input::mouseHandler(std::any window, int button, int action, int mods) {
  for (auto& sub : _subscribers) {
    sub->mouseNotify(window, button, action, mods);
  }
}

void Input::subscribe(std::shared_ptr<InputSubscriber> sub) { _subscribers.push_back(sub); }