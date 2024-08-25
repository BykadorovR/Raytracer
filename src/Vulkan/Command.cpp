#include "Vulkan/Command.h"

CommandBuffer::CommandBuffer(int number, std::shared_ptr<CommandPool> pool, std::shared_ptr<State> state) {
  _pool = pool;
  _state = state;

  _buffer.resize(number);

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = pool->getCommandPool();
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = number;

  if (vkAllocateCommandBuffers(_state->getDevice()->getLogicalDevice(), &allocInfo, _buffer.data()) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate command buffers!");
  }
}

void CommandBuffer::beginCommands() {
  int frameInFlight = _state->getFrameInFlight();

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(_buffer[frameInFlight], &beginInfo);
  _active = true;
}

void CommandBuffer::endCommands() {
  int frameInFlight = _state->getFrameInFlight();
  vkEndCommandBuffer(_buffer[frameInFlight]);
  _active = false;
}

bool CommandBuffer::getActive() { return _active; }

std::vector<VkCommandBuffer>& CommandBuffer::getCommandBuffer() { return _buffer; }

CommandBuffer::~CommandBuffer() {
  _active = false;
  for (auto& buffer : _buffer)
    vkFreeCommandBuffers(_state->getDevice()->getLogicalDevice(), _pool->getCommandPool(), 1, &buffer);
}