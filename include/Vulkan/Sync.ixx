module;
export module Sync;
import "vulkan/vulkan.hpp";
import <memory>;
import Device;

export namespace VulkanEngine {
class Semaphore {
 private:
  std::shared_ptr<Device> _device;
  VkSemaphore _semaphore;
  VkSemaphoreType _type;

 public:
  Semaphore(VkSemaphoreType type, std::shared_ptr<Device> device);
  VkSemaphore& getSemaphore();
  ~Semaphore();
};

class Fence {
 private:
  std::shared_ptr<Device> _device;
  VkFence _fence;

 public:
  Fence(std::shared_ptr<Device> device);
  VkFence& getFence();
  ~Fence();
};
}  // namespace Sync