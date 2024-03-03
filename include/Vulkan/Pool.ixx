module;
export module Pool;
import "vulkan/vulkan.hpp";
import Device;

export namespace VulkanEngine {
class CommandPool {
 private:
  std::shared_ptr<Device> _device;
  QueueType _type;
  VkCommandPool _commandPool;

 public:
  CommandPool(QueueType type, std::shared_ptr<Device> device);
  VkCommandPool& getCommandPool();
  QueueType getType();
  ~CommandPool();
};

class DescriptorPool {
 private:
  std::shared_ptr<Device> _device;
  VkDescriptorPool _descriptorPool;

 public:
  DescriptorPool(int number, std::shared_ptr<Device> device);
  VkDescriptorPool& getDescriptorPool();
  ~DescriptorPool();
};
}  // namespace VulkanEngine