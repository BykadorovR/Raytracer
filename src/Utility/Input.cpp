#include "Input.h"
#include <memory>

static void cursorCallback(GLFWwindow* window, double xpos, double ypos) {
  reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->cursorHandler(window, xpos, ypos);
}

static void mouseCallback(GLFWwindow* window, int button, int action, int mods) {
  reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->mouseHandler(window, button, action, mods);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->keyHandler(window, key, action, mods);
}

Input::Input(std::shared_ptr<Window> window) {
  glfwSetWindowUserPointer(window->getWindow(), this);
  glfwSetInputMode(window->getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(window->getWindow(), cursorCallback);
  glfwSetMouseButtonCallback(window->getWindow(), mouseCallback);
  glfwSetKeyCallback(window->getWindow(), keyCallback);
}

void Input::keyHandler(GLFWwindow* window, int key, int action, int mods) {
  for (auto& sub : _subscribers) {
    sub->keyNotify(window, key, action, mods);
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