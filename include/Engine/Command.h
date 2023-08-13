#pragma once
#include "Device.h"
#include "Sync.h"

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

class CommandBuffer {
 private:
  std::vector<VkCommandBuffer> _buffer;
  std::shared_ptr<Device> _device;
  std::shared_ptr<CommandPool> _pool;

 public:
  CommandBuffer(int size, std::shared_ptr<CommandPool> pool, std::shared_ptr<Device> device);
  void beginCommands(int cmd);
  void endCommands(int cmd, bool blocking = true);
  void endCommands(int cmd, VkSubmitInfo info, std::shared_ptr<Fence> fence);
  std::vector<VkCommandBuffer>& getCommandBuffer();
  ~CommandBuffer();
};