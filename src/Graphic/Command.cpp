#include "Command.h"

CommandPool::CommandPool(std::shared_ptr<Device> device) {
  _device = device;
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = device->getSupportedGraphicsFamilyIndex().value();

  if (vkCreateCommandPool(device->getLogicalDevice(), &poolInfo, nullptr, &_commandPool) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics command pool!");
  }
}

VkCommandPool& CommandPool::getCommandPool() { return _commandPool; }

CommandPool::~CommandPool() { vkDestroyCommandPool(_device->getLogicalDevice(), _commandPool, nullptr); }

CommandBuffer::CommandBuffer(int number, std::shared_ptr<CommandPool> pool, std::shared_ptr<Device> device) {
  _pool = pool;
  _device = device;

  _buffer.resize(number);

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = pool->getCommandPool();
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = number;

  if (vkAllocateCommandBuffers(device->getLogicalDevice(), &allocInfo, _buffer.data()) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate command buffers!");
  }
}

void CommandBuffer::beginSingleTimeCommands(int commandNumber) {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(_buffer[commandNumber], &beginInfo);
}

void CommandBuffer::endSingleTimeCommands(int commandNumber, std::shared_ptr<Queue> queue) {
  vkEndCommandBuffer(_buffer[commandNumber]);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &_buffer[commandNumber];

  vkQueueSubmit(queue->getGraphicQueue(), 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue->getGraphicQueue());
}

std::vector<VkCommandBuffer>& CommandBuffer::getCommandBuffer() { return _buffer; }

CommandBuffer::~CommandBuffer() {
  for (auto& buffer : _buffer) vkFreeCommandBuffers(_device->getLogicalDevice(), _pool->getCommandPool(), 1, &buffer);
}