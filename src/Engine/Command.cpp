#include "Command.h"

CommandPool::CommandPool(QueueType type, std::shared_ptr<Device> device) {
  _device = device;
  _type = type;
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = device->getSupportedFamilyIndex(type).value();

  if (vkCreateCommandPool(device->getLogicalDevice(), &poolInfo, nullptr, &_commandPool) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics command pool!");
  }
}

QueueType CommandPool::getType() { return _type; }

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

void CommandBuffer::beginCommands(int currentFrame) {
  _currentFrame = currentFrame;

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(_buffer[currentFrame], &beginInfo);
}

void CommandBuffer::endCommands() { vkEndCommandBuffer(_buffer[_currentFrame]); }

void CommandBuffer::submitToQueue(bool blocking) {
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &_buffer[_currentFrame];
  auto queue = _device->getQueue(_pool->getType());
  std::unique_lock<std::mutex> lock(_device->getQueueMutex(_pool->getType()));
  vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
  // instead of fence
  if (blocking) vkQueueWaitIdle(queue);
}

void CommandBuffer::submitToQueue(VkSubmitInfo info, std::shared_ptr<Fence> fence) {
  auto queue = _device->getQueue(_pool->getType());
  VkFence currentFence = VK_NULL_HANDLE;
  if (fence) currentFence = fence->getFence();
  std::unique_lock<std::mutex> lock(_device->getQueueMutex(_pool->getType()));
  vkQueueSubmit(queue, 1, &info, currentFence);
}

std::vector<VkCommandBuffer>& CommandBuffer::getCommandBuffer() { return _buffer; }

CommandBuffer::~CommandBuffer() {
  for (auto& buffer : _buffer) vkFreeCommandBuffers(_device->getLogicalDevice(), _pool->getCommandPool(), 1, &buffer);
}