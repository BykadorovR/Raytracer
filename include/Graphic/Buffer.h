#pragma once
#include "Device.h"
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include "Command.h"
#include "Queue.h"

struct Vertex {
  glm::vec2 pos;
  glm::vec3 color;

  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    return attributeDescriptions;
  }
};

class Buffer {
 private:
  VkBuffer _data;
  VkDeviceMemory _memory;
  VkDeviceSize _size;
  std::shared_ptr<Device> _device;

 public:
  Buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, std::shared_ptr<Device> device);
  void copyFrom(std::shared_ptr<Buffer> buffer, std::shared_ptr<CommandPool> commandPool, std::shared_ptr<Queue> queue);
  VkBuffer& getData();
  VkDeviceSize& getSize();
  VkDeviceMemory& getMemory();
  ~Buffer();
};

class VertexBuffer {
 private:
  std::shared_ptr<Buffer> _buffer;

 public:
  VertexBuffer(std::vector<Vertex> vertices,
               std::shared_ptr<CommandPool> commandPool,
               std::shared_ptr<Queue> queue,
               std::shared_ptr<Device> device);
  std::shared_ptr<Buffer> getBuffer();
};

class IndexBuffer {
 private:
  std::shared_ptr<Buffer> _buffer;

 public:
  IndexBuffer(std::vector<uint16_t> indices,
              std::shared_ptr<CommandPool> commandPool,
              std::shared_ptr<Queue> queue,
              std::shared_ptr<Device> device);
  std::shared_ptr<Buffer> getBuffer();
};

class UniformBuffer {
 private:
  std::vector<std::shared_ptr<Buffer>> _buffer;

 public:
  UniformBuffer(int number,
                int size,
                std::shared_ptr<CommandPool> commandPool,
                std::shared_ptr<Queue> queue,
                std::shared_ptr<Device> device);
  std::vector<std::shared_ptr<Buffer>>& getBuffer();
};

class DescriptorSetLayout {
 private:
  std::shared_ptr<Device> _device;
  VkDescriptorSetLayout _descriptorSetLayout;

 public:
  DescriptorSetLayout(std::shared_ptr<Device> device);
  VkDescriptorSetLayout& getDescriptorSetLayout();
  ~DescriptorSetLayout();
};

class DescriptorPool {
 private:
  std::shared_ptr<Device> _device;
  VkDescriptorPool _descriptorPool;

 public:
  DescriptorPool(int number, std::shared_ptr<Device> device);
  VkDescriptorPool& getDescriptorPool();
  ~DescriptorPool();
};

class DescriptorSet {
 private:
  std::vector<VkDescriptorSet> _descriptorSets;

 public:
  DescriptorSet(int number,
                int binding,
                std::shared_ptr<UniformBuffer> uniformBuffer,
                std::shared_ptr<DescriptorSetLayout> layout,
                std::shared_ptr<DescriptorPool> pool,
                std::shared_ptr<Device> device);
  std::vector<VkDescriptorSet> getDescriptorSets();
};