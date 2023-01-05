#include "Input.h"

void Input::initialize(std::shared_ptr<Window> window) {
  static bool initialized = false;
  if (initialized == false) {
    glfwSetInputMode(window->getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window->getWindow(), Input::cursorPositionCallback);
    glfwSetMouseButtonCallback(window->getWindow(), Input::mouseButtonCallback);
    glfwSetKeyCallback(window->getWindow(), Input::keyCallback);
    initialized = true;
  }
}

void Input::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) keyW = true;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) keyS = true;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) keyA = true;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) keyD = true;

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_RELEASE) keyW = false;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_RELEASE) keyS = false;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_RELEASE) keyA = false;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_RELEASE) keyD = false;
}

bool firstMouse = true;
float lastX, lastY;
float yaw = -90.f;
float pitch = 0;
void Input::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
  mousePos = {xpos, ypos};
  if (firstMouse) {
    lastX = xpos;
    lastY = ypos;
    firstMouse = false;
  }

  float xoffset = xpos - lastX;
  float yoffset = lastY - ypos;
  lastX = xpos;
  lastY = ypos;

  float sensitivity = 0.1f;
  xoffset *= sensitivity;
  yoffset *= sensitivity;

  yaw += xoffset;
  pitch += yoffset;

  if (pitch > 89.0f) pitch = 89.0f;
  if (pitch < -89.0f) pitch = -89.0f;

  direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
  direction.y = sin(glm::radians(pitch));
  direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
  direction = glm::normalize(direction);
}

void Input::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
    mouseDownLeft = true;
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
    mouseDownRight = true;
  }
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
    mouseDownLeft = false;
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
    mouseDownRight = false;
  }
}

std::tuple<float, float> Input::mousePos = {0, 0};
bool Input::mouseDownLeft = false;
bool Input::mouseDownRight = false;
bool Input::keyW = false;
bool Input::keyS = false;
bool Input::keyA = false;
bool Input::keyD = false;
glm::vec3 Input::direction = glm::vec3(0, 0, -1);