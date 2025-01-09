#include "Vulkan/Command.h"

CommandBuffer::CommandBuffer(std::shared_ptr<CommandPool> pool, std::shared_ptr<Device> device) {
  _pool = pool;
  _device = device;

  VkCommandBufferAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                        .commandPool = pool->getCommandPool(),
                                        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                        .commandBufferCount = 1};

  if (vkAllocateCommandBuffers(device->getLogicalDevice(), &allocInfo, &_buffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate command buffers!");
  }
}

void CommandBuffer::beginCommands() {
  VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                     .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkBeginCommandBuffer(_buffer, &beginInfo);
  _active = true;
}

void CommandBuffer::endCommands() {
  vkEndCommandBuffer(_buffer);
  _active = false;
}

bool CommandBuffer::getActive() { return _active; }

VkCommandBuffer& CommandBuffer::getCommandBuffer() { return _buffer; }

CommandBuffer::~CommandBuffer() {
  _active = false;
  vkFreeCommandBuffers(_device->getLogicalDevice(), _pool->getCommandPool(), 1, &_buffer);
}