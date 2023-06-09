#pragma once
#include "Device.h"
#include <array>
#include "Command.h"
#include "Queue.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

struct Vertex2D {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 texCoord;
  glm::vec3 tangent;

  bool operator==(const Vertex2D& other) const {
    return pos == other.pos && texCoord == other.texCoord && color == other.color && normal == other.normal &&
           tangent == other.tangent;
  }

  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex2D);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{5};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex2D, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex2D, normal);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex2D, color);

    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Vertex2D, texCoord);

    attributeDescriptions[4].binding = 0;
    attributeDescriptions[4].location = 4;
    attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[4].offset = offsetof(Vertex2D, tangent);

    return attributeDescriptions;
  }
};

struct DepthConstants {
  alignas(16) glm::vec3 lightPosition;
  int far;
  static VkPushConstantRange getPushConstant(int offset) {
    VkPushConstantRange pushConstant;
    // this push constant range starts at the beginning
    pushConstant.offset = offset;
    // this push constant range takes up the size of a MeshPushConstants struct
    pushConstant.size = sizeof(DepthConstants);
    // this push constant range is accessible only in the vertex shader
    pushConstant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    return pushConstant;
  }
};
struct LightPush {
  alignas(16) int test;
  alignas(16) glm::vec3 cameraPosition;
  static VkPushConstantRange getPushConstant() {
    VkPushConstantRange pushConstant;
    // this push constant range starts at the beginning
    pushConstant.offset = 0;
    // this push constant range takes up the size of a MeshPushConstants struct
    pushConstant.size = sizeof(LightPush);
    // this push constant range is accessible only in the vertex shader
    pushConstant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    return pushConstant;
  }
};

struct PushConstants {
  alignas(16) int jointNum = 0;
  static VkPushConstantRange getPushConstant(int offset) {
    VkPushConstantRange pushConstant;
    // this push constant range starts at the beginning
    pushConstant.offset = offset;
    // this push constant range takes up the size of a MeshPushConstants struct
    pushConstant.size = sizeof(PushConstants);
    // this push constant range is accessible only in the vertex shader
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    return pushConstant;
  }
};

struct Vertex3D {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 texCoord;
  glm::vec4 jointIndices;
  glm::vec4 jointWeights;
  glm::vec4 tangent;

  bool operator==(const Vertex3D& other) const {
    return pos == other.pos && normal == other.normal && color == other.color && texCoord == other.texCoord &&
           jointIndices == other.jointIndices && jointWeights == other.jointWeights && tangent == other.tangent;
  }

  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex3D);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::array<VkVertexInputAttributeDescription, 7> getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 7> attributeDescriptions{};

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

    attributeDescriptions[4].binding = 0;
    attributeDescriptions[4].location = 4;
    attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[4].offset = offsetof(Vertex3D, jointIndices);

    attributeDescriptions[5].binding = 0;
    attributeDescriptions[5].location = 5;
    attributeDescriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[5].offset = offsetof(Vertex3D, jointWeights);

    attributeDescriptions[6].binding = 0;
    attributeDescriptions[6].location = 6;
    attributeDescriptions[6].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[6].offset = offsetof(Vertex3D, tangent);

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