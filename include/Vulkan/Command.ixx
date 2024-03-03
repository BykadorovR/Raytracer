module;
export module Command;
import "vulkan/vulkan.hpp";
import Device;
import Sync;
import Pool;
import State;

export namespace VulkanEngine {
class CommandBuffer {
 private:
  std::vector<VkCommandBuffer> _buffer;
  std::shared_ptr<State> _state;
  std::shared_ptr<CommandPool> _pool;

 public:
  CommandBuffer(int size, std::shared_ptr<CommandPool> pool, std::shared_ptr<State> state);
  void beginCommands();
  void endCommands();
  std::vector<VkCommandBuffer>& getCommandBuffer();
  ~CommandBuffer();
};
}  // namespace VulkanEngine