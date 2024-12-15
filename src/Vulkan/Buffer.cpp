#include "Vulkan/Buffer.h"

Buffer::Buffer(VkDeviceSize size,
               VkBufferUsageFlags usage,
               VkMemoryPropertyFlags properties,
               std::shared_ptr<EngineState> engineState) {
  _size = size;
  _engineState = engineState;

  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocCreateInfo = {};
  allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

  vmaCreateBuffer(engineState->getMemoryAllocator()->getAllocator(), &bufferInfo, &allocCreateInfo, &_data, &_memory,
                  &_memoryInfo);
}

void Buffer::copyFrom(std::shared_ptr<Buffer> buffer,
                      VkDeviceSize srcOffset,
                      VkDeviceSize dstOffset,
                      std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  _stagingBuffer = buffer;

  VkBufferCopy copyRegion{};
  copyRegion.size = buffer->getSize() - srcOffset;
  copyRegion.srcOffset = srcOffset;
  copyRegion.dstOffset = dstOffset;
  vkCmdCopyBuffer(commandBufferTransfer->getCommandBuffer()[_engineState->getFrameInFlight()], buffer->getData(), _data,
                  1, &copyRegion);
}

VkDeviceSize& Buffer::getSize() { return _size; }

VkBuffer& Buffer::getData() { return _data; }

VkResult Buffer::flush() {
  return vmaFlushAllocation(_engineState->getMemoryAllocator()->getAllocator(), _memory, 0, VK_WHOLE_SIZE);
}

Buffer::~Buffer() { vmaDestroyBuffer(_engineState->getMemoryAllocator()->getAllocator(), _data, _memory); }

BufferImage::BufferImage(std::tuple<int, int> resolution,
                         int channels,
                         int number,
                         VkBufferUsageFlags usage,
                         VkMemoryPropertyFlags properties,
                         std::shared_ptr<EngineState> engineState)
    : Buffer(std::get<0>(resolution) * std::get<1>(resolution) * channels * number, usage, properties, engineState) {
  _resolution = resolution;
  _channels = channels;
  _number = number;
}

std::tuple<int, int> BufferImage::getResolution() { return _resolution; }

int BufferImage::getChannels() { return _channels; }

int BufferImage::getNumber() { return _number; }