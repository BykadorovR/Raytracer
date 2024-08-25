#pragma once
#include "Vulkan/Device.h"
#include "Vulkan/Sync.h"
#include "Vulkan/Pool.h"
#include "Utility/State.h"

class CommandBuffer {
 private:
  std::vector<VkCommandBuffer> _buffer;
  std::shared_ptr<State> _state;
  std::shared_ptr<CommandPool> _pool;
  bool _active = false;

 public:
  CommandBuffer(int size, std::shared_ptr<CommandPool> pool, std::shared_ptr<State> state);
  void beginCommands();
  void endCommands();
  bool getActive();
  std::vector<VkCommandBuffer>& getCommandBuffer();
  ~CommandBuffer();
};