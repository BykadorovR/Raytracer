#pragma once
#include "Device.h"
#include "Sync.h"
#include "Pool.h"

class CommandBuffer {
 private:
  std::vector<VkCommandBuffer> _buffer;
  std::shared_ptr<Device> _device;
  std::shared_ptr<CommandPool> _pool;
  int _currentFrame;

 public:
  CommandBuffer(int size, std::shared_ptr<CommandPool> pool, std::shared_ptr<Device> device);
  void beginCommands(int currentFrame);
  void endCommands();
  void submitToQueue(bool blocking);
  void submitToQueue(VkSubmitInfo info, std::shared_ptr<Fence> fence);
  std::vector<VkCommandBuffer>& getCommandBuffer();
  int getCurrentFrame();
  ~CommandBuffer();
};