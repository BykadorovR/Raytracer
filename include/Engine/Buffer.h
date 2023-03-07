#pragma once
#include "Device.h"
#include <array>
#include "Command.h"
#include "Queue.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

struct Vertex2D {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 texCoord;

  bool operator==(const Vertex2D& other) const {
    return pos == other.pos && texCoord == other.texCoord && color == other.color;
  }

  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex2D);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex2D, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex2D, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex2D, texCoord);

    return attributeDescriptions;
  }
};

struct Vertex3D {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 texCoord;

  bool operator==(const Vertex3D& other) const {
    return pos == other.pos && normal == other.normal && color == other.color && texCoord == other.texCoord;
  }

  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex3D);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex3D, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex3D, normal);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex3D, color);

    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Vertex3D, texCoord);

    return attributeDescriptions;
  }
};

namespace std {
template <>
struct hash<Vertex2D> {
  size_t operator()(Vertex2D const& vertex) const {
    return (hash<glm::vec2>()(vertex.pos) ^ (hash<glm::vec2>()(vertex.texCoord) >> 1) ^
            (hash<glm::vec3>()(vertex.color) << 1));
  }
};

template <>
struct hash<Vertex3D> {
  size_t operator()(Vertex3D const& vertex) const {
    return (hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.normal) << 1) ^
            (hash<glm::vec2>()(vertex.texCoord) >> 1) ^ (hash<glm::vec3>()(vertex.color) << 1));
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

class VertexBuffer2D {
 private:
  std::shared_ptr<Buffer> _buffer;

 public:
  VertexBuffer2D(std::vector<Vertex2D> vertices,
                 std::shared_ptr<CommandPool> commandPool,
                 std::shared_ptr<Queue> queue,
                 std::shared_ptr<Device> device);
  std::shared_ptr<Buffer> getBuffer();
};

class VertexBuffer3D {
 private:
  std::shared_ptr<Buffer> _buffer;

 public:
  VertexBuffer3D(std::vector<Vertex3D> vertices,
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