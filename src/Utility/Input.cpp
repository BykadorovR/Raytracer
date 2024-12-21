#include "Utility/Input.h"
#include <memory>

Input::Input(std::shared_ptr<Window> window) {
#ifndef __ANDROID__
  _window = window;
  glfwSetWindowUserPointer((GLFWwindow*)(window->getWindow()), this);
  glfwSetInputMode((GLFWwindow*)(window->getWindow()), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  glfwSetCursorPosCallback((GLFWwindow*)(window->getWindow()), [](GLFWwindow* window, double xpos, double ypos) {
    reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->cursorHandler(xpos, ypos);
  });
  glfwSetMouseButtonCallback(
      (GLFWwindow*)(window->getWindow()), [](GLFWwindow* window, int button, int action, int mods) {
        reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->mouseHandler(button, action, mods);
      });
  glfwSetCharCallback((GLFWwindow*)(window->getWindow()), [](GLFWwindow* window, unsigned int code) {
    reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->charHandler(code);
  });
  glfwSetKeyCallback(
      (GLFWwindow*)(window->getWindow()), [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->keyHandler(key, scancode, action, mods);
      });
  glfwSetScrollCallback((GLFWwindow*)(window->getWindow()), [](GLFWwindow* window, double xOffset, double yOffset) {
    reinterpret_cast<Input*>(glfwGetWindowUserPointer(window))->scrollHandler(xOffset, yOffset);
  });
#endif
}

void Input::keyHandler(int key, int scancode, int action, int mods) {
  for (auto& sub : _subscribersExclusive)
    if (sub->keyNotify(key, scancode, action, mods)) return;

  for (auto& sub : _subscribers) {
    sub->keyNotify(key, scancode, action, mods);
  }
}

void Input::charHandler(unsigned int code) {
  for (auto& sub : _subscribersExclusive)
    if (sub->charNotify(code)) return;

  for (auto& sub : _subscribers) {
    sub->charNotify(code);
  }
}

void Input::scrollHandler(double xOffset, double yOffset) {
  if (_showCursor == true)
    for (auto& sub : _subscribersExclusive)
      if (sub->scrollNotify(xOffset, yOffset)) return;

  for (auto& sub : _subscribers) {
    sub->scrollNotify(xOffset, yOffset);
  }
}

void Input::cursorHandler(double xpos, double ypos) {
  if (_showCursor == true)
    for (auto& sub : _subscribersExclusive) sub->cursorNotify(xpos, ypos);

  for (auto& sub : _subscribers) {
    sub->cursorNotify(xpos, ypos);
  }
}

void Input::mouseHandler(int button, int action, int mods) {
  if (_showCursor == true)
    for (auto& sub : _subscribersExclusive)
      if (sub->mouseNotify(button, action, mods)) return;

  for (auto& sub : _subscribers) {
    sub->mouseNotify(button, action, mods);
  }
}

void Input::subscribe(std::shared_ptr<InputSubscriber> sub) { _subscribers.push_back(sub); }

void Input::subscribe(std::shared_ptr<InputSubscriberExclusive> sub) { _subscribersExclusive.push_back(sub); }

void Input::showCursor(bool show) {
  _showCursor = show;
#ifndef __ANDROID__
  if (show)
    glfwSetInputMode((GLFWwindow*)(_window->getWindow()), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  else
    glfwSetInputMode((GLFWwindow*)(_window->getWindow()), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
#endif
}

bool Input::cursorEnabled() { return _showCursor; }