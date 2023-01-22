#include <tuple>
#include "glfw/glfw3.h"
#include "glm/glm.hpp"
#include "Window.h"

class Input {
 public:
  static std::tuple<float, float> mousePos;
  static glm::vec3 direction;
  static bool mouseDownLeft;
  static bool mouseDownRight;
  static bool keyW, keyS, keyA, keyD, keySpace, keyH;

  static void initialize(std::shared_ptr<Window> window);
  static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
  static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
  static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
};