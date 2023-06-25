#pragma once
#include <tuple>
#include "glfw/glfw3.h"
#include "Window.h"

class InputSubscriber {
 public:
  virtual void cursorNotify(GLFWwindow* window, float xPos, float yPos) = 0;
  virtual void mouseNotify(GLFWwindow* window, int button, int action, int mods) = 0;
  virtual void keyNotify(GLFWwindow* window, int key, int action, int mods) = 0;
};

static void cursorCallback(GLFWwindow* window, double xpos, double ypos);
static void mouseCallback(GLFWwindow* window, int button, int action, int mods);
static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

class Input {
 private:
  std::shared_ptr<Window> _window;
  std::vector<std::shared_ptr<InputSubscriber>> _subscribers;

 public:
  Input(std::shared_ptr<Window> window);
  void cursorHandler(GLFWwindow* window, double xpos, double ypos);
  void mouseHandler(GLFWwindow* window, int button, int action, int mods);
  void keyHandler(GLFWwindow* window, int key, int action, int mods);
  void subscribe(std::shared_ptr<InputSubscriber> sub);
};