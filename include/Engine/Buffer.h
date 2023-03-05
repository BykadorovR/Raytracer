#pragma once
#include "Device.h"
#include <array>
#include "Command.h"
#include "Queue.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 texCoord;

  bool operator==(const Vertex& other) const {
    return pos == other.pos && color == other.color && texCoord == other.texCoord;
  }

  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

    return attributeDescriptions;
  }
};

namespace std {
template <>
struct hash<Vertex> {
  size_t operator()(Vertex const& vertex) const {
    return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
           (hash<glm::vec2>()(vertex.texCoord) << 1);
  }
};
}  // namespace std

class Buffer {
 private:
  VkBuffer _data;
  VkDeviceMemory _memory;
  VkDeviceSize _size;
  std::shared_ptr<Device> _device;
  void* _mapped = nullptr;

 public:
  Buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, std::shared_ptr<Device> device);
  void copyFrom(std::shared_ptr<Buffer> buffer, std::shared_ptr<CommandPool> commandPool, std::shared_ptr<Queue> queue);
  VkBuffer& getData();
  VkDeviceSize& getSize();
  VkDeviceMemory& getMemory();
  void map();
  void unmap();
  void flush();
  void* getMappedMemory();
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
  IndexBuffer(std::vector<uint32_t> indices,
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