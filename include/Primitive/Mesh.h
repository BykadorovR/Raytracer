#pragma once
#include "Buffer.h"
#include "State.h"

struct MeshPrimitive {
  int firstIndex;
  int indexCount;
  int materialIndex;
};

struct Vertex2D {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 texCoord;
  glm::vec3 tangent;
};

struct Vertex3D {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 texCoord;
  glm::vec4 jointIndices;
  glm::vec4 jointWeights;
  glm::vec4 tangent;
};

class Mesh {
 protected:
  std::shared_ptr<State> _state;

 public:
  Mesh(std::shared_ptr<State> state);
  virtual VkVertexInputBindingDescription getBindingDescription() = 0;
  virtual std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() = 0;
};

class Mesh3D : public Mesh {
 private:
  std::mutex _accessVertexMutex, _accessIndexMutex;
  std::vector<uint32_t> _indexData;
  std::vector<Vertex3D> _vertexData;
  std::shared_ptr<VertexBuffer<Vertex3D>> _vertexBuffer;
  std::shared_ptr<VertexBuffer<uint32_t>> _indexBuffer;
  std::vector<MeshPrimitive> _primitives;

 public:
  Mesh3D(std::shared_ptr<State> state);

  void setVertices(std::vector<Vertex3D> vertices, std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void setIndexes(std::vector<uint32_t> indexes, std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void setColor(std::vector<glm::vec3> color, std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void setNormal(std::vector<glm::vec3> normal, std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void setPosition(std::vector<glm::vec3> position, std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void addPrimitive(MeshPrimitive primitive);

  const std::vector<uint32_t>& getIndexData();
  const std::vector<Vertex3D>& getVertexData();
  const std::vector<MeshPrimitive>& getPrimitives();
  std::shared_ptr<VertexBuffer<Vertex3D>> getVertexBuffer();
  std::shared_ptr<VertexBuffer<uint32_t>> getIndexBuffer();
  VkVertexInputBindingDescription getBindingDescription();
  std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
  std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions(
      std::vector<std::tuple<VkFormat, uint32_t>> fields);
};

class MeshCube : public Mesh3D {
 public:
  MeshCube(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state);
};

class MeshSphere : public Mesh3D {
 public:
  MeshSphere(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state);
};

class Mesh2D : public Mesh {
 private:
  std::mutex _accessVertexMutex, _accessIndexMutex;
  std::vector<uint32_t> _indexData;
  std::vector<Vertex2D> _vertexData;
  std::shared_ptr<VertexBuffer<Vertex2D>> _vertexBuffer;
  std::shared_ptr<VertexBuffer<uint32_t>> _indexBuffer;
  bool _changedIndex = false;
  bool _changedVertex = false;

 public:
  Mesh2D(std::shared_ptr<State> state);

  void setVertices(std::vector<Vertex2D> vertices, std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void setIndexes(std::vector<uint32_t> indexes, std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void setColor(glm::vec3 color, std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void setNormal(glm::vec3 normal, std::shared_ptr<CommandBuffer> commandBufferTransfer);

  const std::vector<uint32_t>& getIndexData();
  const std::vector<Vertex2D>& getVertexData();
  std::shared_ptr<VertexBuffer<Vertex2D>> getVertexBuffer();
  std::shared_ptr<VertexBuffer<uint32_t>> getIndexBuffer();
  VkVertexInputBindingDescription getBindingDescription();
  std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};