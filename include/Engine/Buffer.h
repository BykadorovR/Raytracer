#pragma once
#include "Device.h"
#include <array>
#include "Command.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

struct BufferMVP {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 projection;
};

struct DepthConstants {
  alignas(16) glm::vec3 lightPosition;
  alignas(16) int far;
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
  std::vector<T> _vertices;
  VkBufferUsageFlagBits _type;

 public:
  VertexBuffer(VkBufferUsageFlagBits type,
               std::shared_ptr<CommandBuffer> commandBufferTransfer,
               std::shared_ptr<Device> device) {
    _device = device;
    _commandBufferTransfer = commandBufferTransfer;
    _type = type;
  }

  std::vector<T> getData() { return _vertices; }

  void setData(std::vector<T> vertices) {
    _vertices = vertices;

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    if (_stagingBuffer == nullptr || bufferSize != _stagingBuffer->getSize()) {
      _stagingBuffer = std::make_shared<Buffer>(
          bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _device);
    }

    void* data;
    vkMapMemory(_device->getLogicalDevice(), _stagingBuffer->getMemory(), 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(_device->getLogicalDevice(), _stagingBuffer->getMemory());
    if (_buffer == nullptr || bufferSize != _buffer->getSize()) {
      _buffer = std::make_shared<Buffer>(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | _type,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _device);
    }

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