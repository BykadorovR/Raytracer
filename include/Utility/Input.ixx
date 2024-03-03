module;
export module Input;
import "glfw/glfw3.h";
import <vector>;
import <memory>;
import Window;

void cursorCallback(GLFWwindow* window, double xpos, double ypos);
void mouseCallback(GLFWwindow* window, int button, int action, int mods);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void charCallback(GLFWwindow* window, unsigned int code);
void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);

export namespace VulkanEngine {
class InputSubscriber {
 public:
  virtual void cursorNotify(GLFWwindow* window, float xPos, float yPos) = 0;
  virtual void mouseNotify(GLFWwindow* window, int button, int action, int mods) = 0;
  virtual void keyNotify(GLFWwindow* window, int key, int scancode, int action, int mods) = 0;
  virtual void charNotify(GLFWwindow* window, unsigned int code) = 0;
  virtual void scrollNotify(GLFWwindow* window, double xOffset, double yOffset) = 0;
};

class Input {
 private:
  std::shared_ptr<Window> _window;
  std::vector<std::shared_ptr<InputSubscriber>> _subscribers;

 public:
  Input(std::shared_ptr<Window> window);
  void cursorHandler(GLFWwindow* window, double xpos, double ypos);
  void mouseHandler(GLFWwindow* window, int button, int action, int mods);
  void keyHandler(GLFWwindow* window, int key, int scancode, int action, int mods);
  void charHandler(GLFWwindow* window, unsigned int code);
  void scrollHandler(GLFWwindow* window, double xOffset, double yOffset);
  void subscribe(std::shared_ptr<InputSubscriber> sub);
};
}  // namespace VulkanEngine