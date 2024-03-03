import "vulkan/vulkan.hpp";
import <memory>;
import Instance;
import Window;

namespace VulkanEngine {
class Surface {
 private:
  VkSurfaceKHR _surface;
  std::shared_ptr<Instance> _instance;

 public:
  Surface(std::shared_ptr<Window> window, std::shared_ptr<Instance> instance);
  const VkSurfaceKHR& getSurface();
  ~Surface();
};
}  // namespace VulkanEngine