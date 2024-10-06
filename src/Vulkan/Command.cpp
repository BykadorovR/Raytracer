#include "Vulkan/Command.h"

CommandBuffer::CommandBuffer(int number, std::shared_ptr<CommandPool> pool, std::shared_ptr<EngineState> engineState) {
  _pool = pool;
  _engineState = engineState;

  _buffer.resize(number);

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = pool->getCommandPool();
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = number;

  if (vkAllocateCommandBuffers(_engineState->getDevice()->getLogicalDevice(), &allocInfo, _buffer.data()) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to allocate command buffers!");
  }
}

void CommandBuffer::beginCommands() {
  int frameInFlight = _engineState->getFrameInFlight();

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(_buffer[frameInFlight], &beginInfo);
  _active = true;
}

void CommandBuffer::endCommands() {
  int frameInFlight = _engineState->getFrameInFlight();
  vkEndCommandBuffer(_buffer[frameInFlight]);
  _active = false;
}

bool CommandBuffer::getActive() { return _active; }

std::vector<VkCommandBuffer>& CommandBuffer::getCommandBuffer() { return _buffer; }

CommandBuffer::~CommandBuffer() {
  _active = false;
  for (auto& buffer : _buffer)
    vkFreeCommandBuffers(_engineState->getDevice()->getLogicalDevice(), _pool->getCommandPool(), 1, &buffer);
}