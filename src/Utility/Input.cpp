#include "Input.h"
#include <memory>

static void cursorCallback(GLFWwindow* window, double xpos, double ypos) {
  reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->cursorHandler(window, xpos, ypos);
}

static void mouseCallback(GLFWwindow* window, int button, int action, int mods) {
  reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->mouseHandler(window, button, action, mods);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->keyHandler(window, key, scancode, action, mods);
}

static void charCallback(GLFWwindow* window, unsigned int code) {
  reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->charHandler(window, code);
}

static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset) {
  reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->scrollHandler(window, xOffset, yOffset);
}

Input::Input(std::shared_ptr<Window> window) {
  glfwSetWindowUserPointer(window->getWindow(), this);
  glfwSetInputMode(window->getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(window->getWindow(), cursorCallback);
  glfwSetMouseButtonCallback(window->getWindow(), mouseCallback);
  glfwSetCharCallback(window->getWindow(), charCallback);
  glfwSetKeyCallback(window->getWindow(), keyCallback);
  glfwSetScrollCallback(window->getWindow(), scrollCallback);
}

void Input::keyHandler(GLFWwindow* window, int key, int scancode, int action, int mods) {
  for (auto& sub : _subscribers) {
    sub->keyNotify(window, key, scancode, action, mods);
  }
}

void Input::charHandler(GLFWwindow* window, unsigned int code) {
  for (auto& sub : _subscribers) {
    sub->charNotify(window, code);
  }
}

void Input::scrollHandler(GLFWwindow* window, double xOffset, double yOffset) {
  for (auto& sub : _subscribers) {
    sub->scrollNotify(window, xOffset, yOffset);
  }
}

void Input::cursorHandler(GLFWwindow* window, double xpos, double ypos) {
  for (auto& sub : _subscribers) {
    sub->cursorNotify(window, xpos, ypos);
  }
}

void Input::mouseHandler(GLFWwindow* window, int button, int action, int mods) {
  for (auto& sub : _subscribers) {
    sub->mouseNotify(window, button, action, mods);
  }
}

void Input::subscribe(std::shared_ptr<InputSubscriber> sub) { _subscribers.push_back(sub); }