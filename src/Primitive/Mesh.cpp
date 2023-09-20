#include "Mesh.h"

Mesh::Mesh(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state) {
  _state = state;
  auto commandPoolTransfer = std::make_shared<CommandPool>(QueueType::GRAPHIC, _state->getDevice());
  _commandBufferTransfer = std::make_shared<CommandBuffer>(1, commandPoolTransfer, _state->getDevice());
}

Mesh3D::Mesh3D(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state)
    : Mesh(commandBufferTransfer, state) {}

void Mesh3D::addVertex(Vertex3D vertex) {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  _vertexData.push_back(vertex);
  _changedVertex = true;
}

void Mesh3D::setVertices(std::vector<Vertex3D> vertices) {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  _vertexData = vertices;
  _changedVertex = true;
}

void Mesh3D::addIndex(uint32_t index) {
  std::unique_lock<std::mutex> accessLock(_accessIndexMutex);
  _indexData.push_back(index);
  _changedIndex = true;
}

void Mesh3D::setIndexes(std::vector<uint32_t> indexes) {
  std::unique_lock<std::mutex> accessLock(_accessIndexMutex);
  _indexData = indexes;
  _changedIndex = true;
}

const std::vector<uint32_t>& Mesh3D::getIndexData() {
  std::unique_lock<std::mutex> accessLock(_accessIndexMutex);
  return _indexData;
}

const std::vector<Vertex3D>& Mesh3D::getVertexData() {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  return _vertexData;
}

std::shared_ptr<VertexBuffer<Vertex3D>> Mesh3D::getVertexBuffer() {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  if (_changedVertex) {
    _changedVertex = false;
    if (_vertexBuffer == nullptr) {
      _vertexBuffer = std::make_shared<VertexBuffer<Vertex3D>>(_vertexData, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                               _commandBufferTransfer, _state->getDevice());
    } else {
      _vertexBuffer->setData(_vertexData);
    }
  }

  return _vertexBuffer;
}

std::shared_ptr<VertexBuffer<uint32_t>> Mesh3D::getIndexBuffer() {
  std::unique_lock<std::mutex> accessLock(_accessIndexMutex);
  if (_changedIndex) {
    _changedIndex = false;
    if (_indexBuffer == nullptr) {
      _indexBuffer = std::make_shared<VertexBuffer<uint32_t>>(_indexData, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                              _commandBufferTransfer, _state->getDevice());
    } else {
      _indexBuffer->setData(_indexData);
    }
  }

  return _indexBuffer;
}

void Mesh3D::setColor(std::vector<glm::vec3> color) {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  if (color.size() == 0) return;
  if (color.size() > 1 && color.size() != _vertexData.size())
    throw std::invalid_argument("Number of colors doesn't match number of vertices");

  glm::vec3 colorValue = color.front();
  for (int i = 0; i < _vertexData.size(); i++) {
    if (i < color.size()) colorValue = color[i];
    _vertexData[i].color = colorValue;
  }
  _changedVertex = true;
}
void Mesh3D::setNormal(std::vector<glm::vec3> normal) {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  if (normal.size() == 0) return;
  if (normal.size() > 1 && normal.size() != _vertexData.size())
    throw std::invalid_argument("Number of normals doesn't match number of vertices");

  glm::vec3 normalValue = normal.front();
  for (int i = 0; i < _vertexData.size(); i++) {
    if (i < normal.size()) normalValue = normal[i];
    _vertexData[i].normal = normalValue;
  }
  _changedVertex = true;
}

void Mesh3D::setPosition(std::vector<glm::vec3> position) {
  if (position.size() == 0) return;
  if (position.size() > 1 && position.size() != _vertexData.size())
    throw std::invalid_argument("Number of positions doesn't match number of vertices");

  glm::vec3 positionValue = position.front();
  for (int i = 0; i < _vertexData.size(); i++) {
    if (i < position.size()) positionValue = position[i];
    _vertexData[i].pos = positionValue;
  }
  _changedVertex = true;
}

void Mesh3D::addPrimitive(MeshPrimitive primitive) { _primitives.push_back(primitive); }

const std::vector<MeshPrimitive>& Mesh3D::getPrimitives() { return _primitives; }

VkVertexInputBindingDescription Mesh3D::getBindingDescription() {
  VkVertexInputBindingDescription bindingDescription{};
  bindingDescription.binding = 0;
  bindingDescription.stride = sizeof(Vertex3D);
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> Mesh3D::getAttributeDescriptions() {
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions(7);

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

Mesh2D::Mesh2D(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state)
    : Mesh(commandBufferTransfer, state) {}

void Mesh2D::addVertex(Vertex2D vertex) {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  _vertexData.push_back(vertex);
  _changedVertex = true;
}

void Mesh2D::setVertices(std::vector<Vertex2D> vertices) {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  _vertexData = vertices;
  _changedVertex = true;
}

void Mesh2D::addIndex(uint32_t index) {
  std::unique_lock<std::mutex> accessLock(_accessIndexMutex);
  _indexData.push_back(index);
  _changedIndex = true;
}

void Mesh2D::setIndexes(std::vector<uint32_t> indexes) {
  std::unique_lock<std::mutex> accessLock(_accessIndexMutex);
  _indexData = indexes;
  _changedIndex = true;
}

void Mesh2D::setColor(glm::vec3 color) {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  for (auto& vertex : _vertexData) {
    vertex.color = color;
  }

  _changedVertex = true;
}

void Mesh2D::setNormal(glm::vec3 normal) {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  for (auto& vertex : _vertexData) {
    vertex.normal = normal;
  }

  _changedVertex = true;
}

const std::vector<uint32_t>& Mesh2D::getIndexData() {
  std::unique_lock<std::mutex> accessLock(_accessIndexMutex);
  return _indexData;
}

const std::vector<Vertex2D>& Mesh2D::getVertexData() {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  return _vertexData;
}

std::shared_ptr<VertexBuffer<Vertex2D>> Mesh2D::getVertexBuffer() {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  if (_changedVertex) {
    _changedVertex = false;
    if (_vertexBuffer == nullptr) {
      _vertexBuffer = std::make_shared<VertexBuffer<Vertex2D>>(_vertexData, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                               _commandBufferTransfer, _state->getDevice());
    } else {
      _vertexBuffer->setData(_vertexData);
    }
  }

  return _vertexBuffer;
}

std::shared_ptr<VertexBuffer<uint32_t>> Mesh2D::getIndexBuffer() {
  std::unique_lock<std::mutex> accessLock(_accessIndexMutex);
  if (_changedIndex) {
    _changedIndex = false;
    if (_indexBuffer == nullptr) {
      _indexBuffer = std::make_shared<VertexBuffer<uint32_t>>(_indexData, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                              _commandBufferTransfer, _state->getDevice());
    } else {
      _indexBuffer->setData(_indexData);
    }
  }

  return _indexBuffer;
}

VkVertexInputBindingDescription Mesh2D::getBindingDescription() {
  VkVertexInputBindingDescription bindingDescription{};
  bindingDescription.binding = 0;
  bindingDescription.stride = sizeof(Vertex2D);
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> Mesh2D::getAttributeDescriptions() {
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