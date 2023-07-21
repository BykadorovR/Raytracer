#pragma once
#include "Device.h"
#include <array>
#include "Command.h"
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
  alignas(16) int enableShadow;
  alignas(16) int enableLighting;
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
  void copyFrom(std::shared_ptr<Buffer> buffer, std::shared_ptr<CommandBuffer> commandBufferTransfer);
  VkBuffer& getData();
  VkDeviceSize& getSize();
  VkDeviceMemory& getMemory();
  void map();
  void unmap();
  void flush();
  void* getMappedMemory();
  ~Buffer();
};

template <class T>
class VertexBuffer {
 private:
  std::shared_ptr<Buffer> _buffer, _stagingBuffer;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;
  std::shared_ptr<Device> _device;

 public:
  VertexBuffer(std::vector<T> vertices,
               VkBufferUsageFlagBits type,
               std::shared_ptr<CommandBuffer> commandBufferTransfer,
               std::shared_ptr<Device> device) {
    _device = device;
    _commandBufferTransfer = commandBufferTransfer;

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    _stagingBuffer = std::make_shared<Buffer>(
        bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, device);

    void* data;
    vkMapMemory(device->getLogicalDevice(), _stagingBuffer->getMemory(), 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device->getLogicalDevice(), _stagingBuffer->getMemory());

    _buffer = std::make_shared<Buffer>(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | type,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, device);

    _buffer->copyFrom(_stagingBuffer, commandBufferTransfer);
  }

  void setData(std::vector<T> vertices) {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    if (bufferSize != _stagingBuffer->getSize()) throw std::runtime_error("Buffer size should be the same");

    void* data;
    vkMapMemory(_device->getLogicalDevice(), _stagingBuffer->getMemory(), 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(_device->getLogicalDevice(), _stagingBuffer->getMemory());
    _buffer->copyFrom(_stagingBuffer, _commandBufferTransfer);
  }

  std::shared_ptr<Buffer> getBuffer() { return _buffer; }
};

class UniformBuffer {
 private:
  std::vector<std::shared_ptr<Buffer>> _buffer;

 public:
  UniformBuffer(int number, int size, std::shared_ptr<Device> device);
  std::vector<std::shared_ptr<Buffer>>& getBuffer();
};