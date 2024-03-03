module;
#define GLFW_INCLUDE_VULKAN
export module Window;
import "GLFW/glfw3.h";
import <tuple>;
import <vector>;

export namespace VulkanEngine {
class Window {
 private:
  GLFWwindow* _window;
  std::tuple<int, int> _resolution;
  bool _resized = false;

 public:
  Window(std::tuple<int, int> resolution);
  GLFWwindow* getWindow();
  bool getResized();
  void setResized(bool resized);
  std::vector<const char*> getExtensions();

  ~Window();
};
}  // namespace VulkanEngine