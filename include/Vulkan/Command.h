#pragma once
#include "Vulkan/Device.h"
#include "Vulkan/Pool.h"

class CommandBuffer {
 private:
  VkCommandBuffer _buffer;
  std::shared_ptr<Device> _device;
  std::shared_ptr<CommandPool> _pool;
  bool _active = false;

 public:
  CommandBuffer(std::shared_ptr<CommandPool> pool, std::shared_ptr<Device> device);
  void beginCommands();
  void endCommands();
  bool getActive();
  VkCommandBuffer& getCommandBuffer();
  ~CommandBuffer();
};