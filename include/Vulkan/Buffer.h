#pragma once
#include "Vulkan/Device.h"
#include "Vulkan/Command.h"
#include <array>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#undef far

class Buffer {
 private:
  VkBuffer _data;
  VkDeviceMemory _memory;
  VkDeviceSize _size;
  std::shared_ptr<EngineState> _engineState;
  void* _mapped = nullptr;
  std::shared_ptr<Buffer> _stagingBuffer;

 public:
  Buffer(VkDeviceSize size,
         VkBufferUsageFlags usage,
         VkMemoryPropertyFlags properties,
         std::shared_ptr<EngineState> engineState);
  void copyFrom(std::shared_ptr<Buffer> buffer,
                VkDeviceSize srcOffset,
                VkDeviceSize dstOffset,
                std::shared_ptr<CommandBuffer> commandBufferTransfer);
  template <class T>
  void setData(T* data, VkDeviceSize size, int offset = 0) {
    map();
    memcpy((char*)_mapped + offset, data, size);
    unmap();
  }

  template <class T>
  void setData(T* data) {
    setData(data, _size);
  }
  VkBuffer& getData();
  VkDeviceSize& getSize();
  VkDeviceMemory& getMemory();
  void map();
  void unmap();
  void flush();
  void* getMappedMemory();
  ~Buffer();
};

class BufferImage : public Buffer {
 private:
  std::tuple<int, int> _resolution;
  int _channels;
  int _number;

 public:
  BufferImage(std::tuple<int, int> resolution,
              int channels,
              int number,
              VkBufferUsageFlags usage,
              VkMemoryPropertyFlags properties,
              std::shared_ptr<EngineState> engineState);
  std::tuple<int, int> getResolution();
  int getChannels();
  int getNumber();
};

template <class T>
class VertexBuffer {
 protected:
  std::shared_ptr<Buffer> _buffer;
  std::shared_ptr<EngineState> _engineState;
  std::vector<T> _vertices;
  VkBufferUsageFlagBits _type;

 public:
  VertexBuffer(VkBufferUsageFlagBits type, std::shared_ptr<EngineState> engineState) {
    _engineState = engineState;
    _type = type;
  }
  std::vector<T> getData() { return _vertices; }
  std::shared_ptr<Buffer> getBuffer() { return _buffer; }
};

template <class T>
class VertexBufferDynamic : public VertexBuffer<T> {
 public:
  VertexBufferDynamic(VkBufferUsageFlagBits type, std::shared_ptr<EngineState> engineState)
      : VertexBuffer<T>(type, engineState) {}
  void setData(std::vector<T> vertices) {
    this->_vertices = vertices;

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    if (this->_buffer == nullptr || bufferSize != this->_buffer->getSize()) {
      this->_buffer = std::make_shared<Buffer>(
          bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | this->_type,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, this->_engineState);
    }

    this->_buffer->setData(vertices.data());
  }
};

template <class T>
class VertexBufferStatic : public VertexBuffer<T> {
 private:
  std::shared_ptr<Buffer> _stagingBuffer;

 public:
  VertexBufferStatic(VkBufferUsageFlagBits type, std::shared_ptr<EngineState> engineState)
      : VertexBuffer<T>(type, engineState) {}
  void setData(std::vector<T> vertices, std::shared_ptr<CommandBuffer> commandBufferTransfer) {
    this->_vertices = vertices;

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    if (_stagingBuffer == nullptr || bufferSize != _stagingBuffer->getSize()) {
      _stagingBuffer = std::make_shared<Buffer>(
          bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, this->_engineState);
    }
    _stagingBuffer->setData(vertices.data());
    if (this->_buffer == nullptr || bufferSize != this->_buffer->getSize()) {
      this->_buffer = std::make_shared<Buffer>(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | this->_type,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, this->_engineState);
    }

    this->_buffer->copyFrom(_stagingBuffer, 0, 0, commandBufferTransfer);
    // need to insert memory barrier so read in vertex shader waits for copy
    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.pNext = nullptr;
    memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    vkCmdPipelineBarrier(commandBufferTransfer->getCommandBuffer()[this->_engineState->getFrameInFlight()],
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 1, &memoryBarrier, 0,
                         nullptr, 0, nullptr);
  }
};