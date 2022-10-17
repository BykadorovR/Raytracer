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
    }
  }
  if (memoryID < 0) throw std::runtime_error("failed to find suitable memory type!");

  allocInfo.memoryTypeIndex = memoryID;

  if (vkAllocateMemory(device->getLogicalDevice(), &allocInfo, nullptr, &_memory) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate buffer memory!");
  }

  vkBindBufferMemory(device->getLogicalDevice(), _data, _memory, 0);
}

void Buffer::copyFrom(std::shared_ptr<Buffer> buffer,
                      std::shared_ptr<CommandPool> commandPool,
                      std::shared_ptr<Queue> queue) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = commandPool->getCommandPool();
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(_device->getLogicalDevice(), &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  VkBufferCopy copyRegion{};
  copyRegion.size = _size;
  vkCmdCopyBuffer(commandBuffer, buffer->getData(), _data, 1, &copyRegion);

  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(queue->getGraphicQueue(), 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue->getGraphicQueue());

  vkFreeCommandBuffers(_device->getLogicalDevice(), commandPool->getCommandPool(), 1, &commandBuffer);
}

VkDeviceSize& Buffer::getSize() { return _size; }

VkBuffer& Buffer::getData() { return _data; }

VkDeviceMemory& Buffer::getMemory() { return _memory; }

Buffer::~Buffer() {
  vkDestroyBuffer(_device->getLogicalDevice(), _data, nullptr);
  vkFreeMemory(_device->getLogicalDevice(), _memory, nullptr);
}

VertexBuffer::VertexBuffer(std::vector<Vertex> vertices,
                           std::shared_ptr<CommandPool> commandPool,
                           std::shared_ptr<Queue> queue,
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

  _buffer->copyFrom(stagingBuffer, commandPool, queue);
}

std::shared_ptr<Buffer> VertexBuffer::getBuffer() { return _buffer; }

IndexBuffer::IndexBuffer(std::vector<uint16_t> indices,
                         std::shared_ptr<CommandPool> commandPool,
                         std::shared_ptr<Queue> queue,
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

  _buffer->copyFrom(stagingBuffer, commandPool, queue);
}

std::shared_ptr<Buffer> IndexBuffer::getBuffer() { return _buffer; }

UniformBuffer::UniformBuffer(int number,
                             int size,
                             std::shared_ptr<CommandPool> commandPool,
                             std::shared_ptr<Queue> queue,
                             std::shared_ptr<Device> device) {
  _buffer.resize(number);
  VkDeviceSize bufferSize = size;

  for (int i = 0; i < number; i++)
    _buffer[i] = std::make_shared<Buffer>(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                          device);
}

std::vector<std::shared_ptr<Buffer>>& UniformBuffer::getBuffer() { return _buffer; }

DescriptorSetLayout::DescriptorSetLayout(std::shared_ptr<Device> device) {
  _device = device;
  VkDescriptorSetLayoutBinding uboLayoutBinding{};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding.pImmutableSamplers = nullptr;
  uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &uboLayoutBinding;

  if (vkCreateDescriptorSetLayout(device->getLogicalDevice(), &layoutInfo, nullptr, &_descriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

VkDescriptorSetLayout& DescriptorSetLayout::getDescriptorSetLayout() { return _descriptorSetLayout; }

DescriptorSetLayout::~DescriptorSetLayout() {
  vkDestroyDescriptorSetLayout(_device->getLogicalDevice(), _descriptorSetLayout, nullptr);
}

DescriptorPool::DescriptorPool(int number, std::shared_ptr<Device> device) {
  _device = device;

  VkDescriptorPoolSize poolSize{};
  poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSize.descriptorCount = static_cast<uint32_t>(number);

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;
  poolInfo.maxSets = static_cast<uint32_t>(number);

  if (vkCreateDescriptorPool(device->getLogicalDevice(), &poolInfo, nullptr, &_descriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }
}

VkDescriptorPool& DescriptorPool::getDescriptorPool() { return _descriptorPool; }

DescriptorPool::~DescriptorPool() { vkDestroyDescriptorPool(_device->getLogicalDevice(), _descriptorPool, nullptr); }

DescriptorSet::DescriptorSet(int number,
                             int binding,
                             std::shared_ptr<UniformBuffer> uniformBuffer,
                             std::shared_ptr<DescriptorSetLayout> layout,
                             std::shared_ptr<DescriptorPool> pool,
                             std::shared_ptr<Device> device) {
  std::vector<VkDescriptorSetLayout> layouts(number, layout->getDescriptorSetLayout());
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = pool->getDescriptorPool();
  allocInfo.descriptorSetCount = static_cast<uint32_t>(number);
  allocInfo.pSetLayouts = layouts.data();

  _descriptorSets.resize(number);
  if (vkAllocateDescriptorSets(device->getLogicalDevice(), &allocInfo, _descriptorSets.data()) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate descriptor sets!");
  }

  for (size_t i = 0; i < number; i++) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffer->getBuffer()[i]->getData();
    bufferInfo.offset = 0;
    bufferInfo.range = uniformBuffer->getBuffer()[i]->getSize();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = _descriptorSets[i];
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device->getLogicalDevice(), 1, &descriptorWrite, 0, nullptr);
  }
}

std::vector<VkDescriptorSet> DescriptorSet::getDescriptorSets() { return _descriptorSets; }