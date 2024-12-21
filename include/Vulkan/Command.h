#pragma once
#include "Vulkan/Device.h"
#include "Vulkan/Sync.h"
#include "Vulkan/Pool.h"
#include "Utility/EngineState.h"

class CommandBuffer {
 private:
  std::vector<VkCommandBuffer> _buffer;
  std::shared_ptr<EngineState> _engineState;
  std::shared_ptr<CommandPool> _pool;
  bool _active = false;

 public:
  CommandBuffer(int size, std::shared_ptr<CommandPool> pool, std::shared_ptr<EngineState> engineState);
  void beginCommands();
  void endCommands();
  bool getActive();
  std::vector<VkCommandBuffer>& getCommandBuffer();
  ~CommandBuffer();
};