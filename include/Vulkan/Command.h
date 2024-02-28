#pragma once
#include "Device.h"
#include "Sync.h"
#include "Pool.h"
#include "State.h"

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