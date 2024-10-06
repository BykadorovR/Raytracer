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

  if (vkCreateBuffer(engineState->getDevice()->getLogicalDevice(), &bufferInfo, nullptr, &_data) != VK_SUCCESS) {
    throw std::runtime_error("failed to create buffer!");
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(engineState->getDevice()->getLogicalDevice(), _data, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(engineState->getDevice()->getPhysicalDevice(), &memProperties);

  int memoryID = -1;
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((memRequirements.memoryTypeBits & (1 << i)) &&
        (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      memoryID = i;
      break;
    }
  }
  if (memoryID < 0) throw std::runtime_error("failed to find suitable memory type!");

  allocInfo.memoryTypeIndex = memoryID;

  if (vkAllocateMemory(engineState->getDevice()->getLogicalDevice(), &allocInfo, nullptr, &_memory) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate buffer memory!");
  }

  vkBindBufferMemory(engineState->getDevice()->getLogicalDevice(), _data, _memory, 0);
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

VkDeviceMemory& Buffer::getMemory() { return _memory; }

// VK_WHOLE_SIZE is used because otherwise memory must be aligned manually to 64 or something
// so can't just use _size
// TODO: try to align memory manually?
void Buffer::map() {
  vkMapMemory(_engineState->getDevice()->getLogicalDevice(), _memory, 0, VK_WHOLE_SIZE, 0, &_mapped);
}

void Buffer::flush() {
  VkMappedMemoryRange mappedRange = {};
  mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  mappedRange.memory = _memory;
  mappedRange.offset = 0;
  mappedRange.size = VK_WHOLE_SIZE;
  vkFlushMappedMemoryRanges(_engineState->getDevice()->getLogicalDevice(), 1, &mappedRange);
}

void Buffer::unmap() {
  if (_mapped != nullptr) {
    vkUnmapMemory(_engineState->getDevice()->getLogicalDevice(), _memory);
    _mapped = nullptr;
  }
}

void* Buffer::getMappedMemory() { return _mapped; }

Buffer::~Buffer() {
  if (_mapped != nullptr) {
    vkUnmapMemory(_engineState->getDevice()->getLogicalDevice(), _memory);
    _mapped = nullptr;
  }
  vkDestroyBuffer(_engineState->getDevice()->getLogicalDevice(), _data, nullptr);
  vkFreeMemory(_engineState->getDevice()->getLogicalDevice(), _memory, nullptr);
}

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

UniformBuffer::UniformBuffer(int number, int size, std::shared_ptr<EngineState> engineState) {
  _buffer.resize(number);
  VkDeviceSize bufferSize = size;

  for (int i = 0; i < number; i++)
    _buffer[i] = std::make_shared<Buffer>(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                          engineState);
}

std::vector<std::shared_ptr<Buffer>>& UniformBuffer::getBuffer() { return _buffer; }