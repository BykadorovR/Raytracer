#pragma once
#include "Vulkan/Buffer.h"
#include "Utility/EngineState.h"

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
  glm::vec4 tangent;
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

class AABB {
 private:
  glm::vec3 _min;
  glm::vec3 _max;

 public:
  AABB();
  void extend(glm::vec3 point);
  void extend(std::shared_ptr<AABB> aabb);
  glm::vec3 getMin();
  glm::vec3 getMax();
};

class Mesh {
 protected:
  std::shared_ptr<EngineState> _engineState;

 public:
  Mesh(std::shared_ptr<EngineState> engineState);
  std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions(
      std::vector<std::tuple<VkFormat, uint32_t>> fields);
  virtual VkVertexInputBindingDescription getBindingDescription() = 0;
  virtual std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() = 0;
};

class MeshDynamic3D : public Mesh {
 private:
  std::mutex _accessVertexMutex, _accessIndexMutex;
  std::vector<uint32_t> _indexData;
  std::vector<Vertex3D> _vertexData;
  std::shared_ptr<VertexBufferDynamic<Vertex3D>> _vertexBuffer;
  std::shared_ptr<VertexBufferDynamic<uint32_t>> _indexBuffer;
  std::vector<MeshPrimitive> _primitives;
  std::shared_ptr<AABB> _aabb;

 public:
  MeshDynamic3D(std::shared_ptr<EngineState> engineState);

  void setVertices(std::vector<Vertex3D> vertices);
  void setIndexes(std::vector<uint32_t> indexes);
  void setColor(std::vector<glm::vec3> color);
  void setNormal(std::vector<glm::vec3> normal);
  void setPosition(std::vector<glm::vec3> position);
  void setAABB(std::shared_ptr<AABB> aabb);
  void addPrimitive(MeshPrimitive primitive);

  const std::vector<uint32_t>& getIndexData();
  const std::vector<Vertex3D>& getVertexData();
  const std::vector<MeshPrimitive>& getPrimitives();
  std::shared_ptr<AABB> getAABB();
  std::shared_ptr<VertexBuffer<Vertex3D>> getVertexBuffer();
  std::shared_ptr<VertexBuffer<uint32_t>> getIndexBuffer();
  VkVertexInputBindingDescription getBindingDescription();
  std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

class MeshStatic3D : public Mesh {
 private:
  std::mutex _accessVertexMutex, _accessIndexMutex;
  std::vector<uint32_t> _indexData;
  std::vector<Vertex3D> _vertexData;
  std::shared_ptr<VertexBufferStatic<Vertex3D>> _vertexBuffer;
  std::shared_ptr<VertexBufferStatic<uint32_t>> _indexBuffer;
  std::vector<MeshPrimitive> _primitives;
  std::shared_ptr<AABB> _aabb;

 public:
  MeshStatic3D(std::shared_ptr<EngineState> engineState);

  void setVertices(std::vector<Vertex3D> vertices, std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void setIndexes(std::vector<uint32_t> indexes, std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void setColor(std::vector<glm::vec3> color, std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void setNormal(std::vector<glm::vec3> normal, std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void setPosition(std::vector<glm::vec3> position, std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void setAABB(std::shared_ptr<AABB> aabb);
  void addPrimitive(MeshPrimitive primitive);

  const std::vector<uint32_t>& getIndexData();
  const std::vector<Vertex3D>& getVertexData();
  const std::vector<MeshPrimitive>& getPrimitives();
  std::shared_ptr<AABB> getAABB();
  std::shared_ptr<VertexBuffer<Vertex3D>> getVertexBuffer();
  std::shared_ptr<VertexBuffer<uint32_t>> getIndexBuffer();
  VkVertexInputBindingDescription getBindingDescription();
  std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

class MeshBoundingBox : public MeshStatic3D {
 public:
  MeshBoundingBox(glm::vec3 minPoint,
                  glm::vec3 maxPoint,
                  std::shared_ptr<CommandBuffer> commandBufferTransfer,
                  std::shared_ptr<EngineState> engineState);
};

class MeshCube : public MeshStatic3D {
 public:
  MeshCube(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<EngineState> engineState);
};

class MeshSphere : public MeshStatic3D {
 public:
  MeshSphere(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<EngineState> engineState);
};

class MeshStatic2D : public Mesh {
 private:
  std::mutex _accessVertexMutex, _accessIndexMutex;
  std::vector<uint32_t> _indexData;
  std::vector<Vertex2D> _vertexData;
  std::shared_ptr<VertexBufferStatic<Vertex2D>> _vertexBuffer;
  std::shared_ptr<VertexBufferStatic<uint32_t>> _indexBuffer;
  bool _changedIndex = false;
  bool _changedVertex = false;

 public:
  MeshStatic2D(std::shared_ptr<EngineState> engineState);

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