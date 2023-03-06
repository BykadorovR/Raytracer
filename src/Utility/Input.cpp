#include "Input.h"
#include <memory>

static void cursorCallback(GLFWwindow* window, double xpos, double ypos) {
  reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->cursorHandler(xpos, ypos);
}

static void mouseCallback(GLFWwindow* window, int button, int action, int mods) {
  reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->mouseHandler(button, action, mods);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->keyHandler(key, action, mods);
}

Input::Input(std::shared_ptr<Window> window) {
  glfwSetWindowUserPointer(window->getWindow(), this);
  glfwSetInputMode(window->getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(window->getWindow(), cursorCallback);
  glfwSetMouseButtonCallback(window->getWindow(), mouseCallback);
  glfwSetKeyCallback(window->getWindow(), keyCallback);
}

void Input::keyHandler(int key, int action, int mods) {
  for (auto& sub : _subscribers) {
    sub->keyNotify(key, action, mods);
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