#include "Buffer.h"

Buffer::Buffer(VkDeviceSize size,
               VkBufferUsageFlags usage,
               VkMemoryPropertyFlags properties,
               std::shared_ptr<Device> device) {
  _size = size;
  _device = device;

  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(device->getLogicalDevice(), &bufferInfo, nullptr, &_data) != VK_SUCCESS) {
    throw std::runtime_error("failed to create buffer!");
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(device->getLogicalDevice(), _data, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(device->getPhysicalDevice(), &memProperties);

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

  if (vkAllocateMemory(device->getLogicalDevice(), &allocInfo, nullptr, &_memory) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate buffer memory!");
  }

  vkBindBufferMemory(device->getLogicalDevice(), _data, _memory, 0);
}

void Buffer::copyFrom(std::shared_ptr<Buffer> buffer, std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  commandBufferTransfer->beginCommands(0);

  VkBufferCopy copyRegion{};
  copyRegion.size = _size;
  vkCmdCopyBuffer(commandBufferTransfer->getCommandBuffer()[0], buffer->getData(), _data, 1, &copyRegion);

  commandBufferTransfer->endCommands(0);
}

VkDeviceSize& Buffer::getSize() { return _size; }

VkBuffer& Buffer::getData() { return _data; }

VkDeviceMemory& Buffer::getMemory() { return _memory; }

void Buffer::map() { vkMapMemory(_device->getLogicalDevice(), _memory, 0, VK_WHOLE_SIZE, 0, &_mapped); }

void Buffer::flush() {
  VkMappedMemoryRange mappedRange = {};
  mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  mappedRange.memory = _memory;
  mappedRange.offset = 0;
  mappedRange.size = VK_WHOLE_SIZE;
  vkFlushMappedMemoryRanges(_device->getLogicalDevice(), 1, &mappedRange);
}

void Buffer::unmap() {
  if (_mapped != nullptr) {
    vkUnmapMemory(_device->getLogicalDevice(), _memory);
    _mapped = nullptr;
  }
}

void* Buffer::getMappedMemory() { return _mapped; }

Buffer::~Buffer() {
  if (_mapped != nullptr) {
    vkUnmapMemory(_device->getLogicalDevice(), _memory);
    _mapped = nullptr;
  }
  vkDestroyBuffer(_device->getLogicalDevice(), _data, nullptr);
  vkFreeMemory(_device->getLogicalDevice(), _memory, nullptr);
}

VertexBuffer2D::VertexBuffer2D(std::vector<Vertex2D> vertices,
                               std::shared_ptr<CommandBuffer> commandBufferTransfer,
                               std::shared_ptr<Device> device) {
  VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

  auto stagingBuffer = std::make_shared<Buffer>(
      bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, device);

  void* data;
  vkMapMemory(device->getLogicalDevice(), stagingBuffer->getMemory(), 0, bufferSize, 0, &data);
  memcpy(data, vertices.data(), (size_t)bufferSize);
  vkUnmapMemory(device->getLogicalDevice(), stagingBuffer->getMemory());

  _buffer = std::make_shared<Buffer>(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, device);

  _buffer->copyFrom(stagingBuffer, commandBufferTransfer);
}

std::shared_ptr<Buffer> VertexBuffer2D::getBuffer() { return _buffer; }

VertexBuffer3D::VertexBuffer3D(std::vector<Vertex3D> vertices,
                               std::shared_ptr<CommandBuffer> commandBufferTransfer,
                               std::shared_ptr<Device> device) {
  VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

  auto stagingBuffer = std::make_shared<Buffer>(
      bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, device);

  void* data;
  vkMapMemory(device->getLogicalDevice(), stagingBuffer->getMemory(), 0, bufferSize, 0, &data);
  memcpy(data, vertices.data(), (size_t)bufferSize);
  vkUnmapMemory(device->getLogicalDevice(), stagingBuffer->getMemory());

  _buffer = std::make_shared<Buffer>(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, device);

  _buffer->copyFrom(stagingBuffer, commandBufferTransfer);
}

std::shared_ptr<Buffer> VertexBuffer3D::getBuffer() { return _buffer; }

IndexBuffer::IndexBuffer(std::vector<uint32_t> indices,
                         std::shared_ptr<CommandBuffer> commandBufferTransfer,
                         std::shared_ptr<Device> device) {
  VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

  auto stagingBuffer = std::make_shared<Buffer>(
      bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, device);

  void* data;
  vkMapMemory(device->getLogicalDevice(), stagingBuffer->getMemory(), 0, bufferSize, 0, &data);
  memcpy(data, indices.data(), (size_t)bufferSize);
  vkUnmapMemory(device->getLogicalDevice(), stagingBuffer->getMemory());

  _buffer = std::make_shared<Buffer>(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, device);

  _buffer->copyFrom(stagingBuffer, commandBufferTransfer);
}

std::shared_ptr<Buffer> IndexBuffer::getBuffer() { return _buffer; }

UniformBuffer::UniformBuffer(int number, int size, std::shared_ptr<Device> device) {
  _buffer.resize(number);
  VkDeviceSize bufferSize = size;

  for (int i = 0; i < number; i++)
    _buffer[i] = std::make_shared<Buffer>(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                          device);
}

std::vector<std::shared_ptr<Buffer>>& UniformBuffer::getBuffer() { return _buffer; }