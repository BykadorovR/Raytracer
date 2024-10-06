#include "Primitive/Mesh.h"
#define _USE_MATH_DEFINES
#include <math.h>

AABB::AABB() {
  _min = glm::vec3(std::numeric_limits<float>::max());
  _max = glm::vec3(-std::numeric_limits<float>::max());
}

void AABB::extend(glm::vec3 point) {
  for (int i = 0; i < _min.length(); i++) _min[i] = std::min(_min[i], point[i]);
  for (int i = 0; i < _max.length(); i++) _max[i] = std::max(_max[i], point[i]);
}

void AABB::extend(std::shared_ptr<AABB> aabb) {
  extend(aabb->getMin());
  extend(aabb->getMax());
}

glm::vec3 AABB::getMin() { return _min; }

glm::vec3 AABB::getMax() { return _max; }

Mesh::Mesh(std::shared_ptr<EngineState> engineState) { _engineState = engineState; }

std::vector<VkVertexInputAttributeDescription> Mesh::getAttributeDescriptions(
    std::vector<std::tuple<VkFormat, uint32_t>> fields) {
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions(fields.size());
  for (int i = 0; i < fields.size(); i++) {
    attributeDescriptions[i].binding = 0;
    attributeDescriptions[i].location = i;
    attributeDescriptions[i].format = std::get<0>(fields[i]);
    attributeDescriptions[i].offset = std::get<1>(fields[i]);
  }
  return attributeDescriptions;
}

MeshStatic3D::MeshStatic3D(std::shared_ptr<EngineState> engineState) : Mesh(engineState) {
  _vertexBuffer = std::make_shared<VertexBufferStatic<Vertex3D>>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, _engineState);
  _indexBuffer = std::make_shared<VertexBufferStatic<uint32_t>>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, _engineState);
}

void MeshStatic3D::setVertices(std::vector<Vertex3D> vertices, std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  _vertexData = vertices;
  _vertexBuffer->setData(_vertexData, commandBufferTransfer);
}

void MeshStatic3D::setIndexes(std::vector<uint32_t> indexes, std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  std::unique_lock<std::mutex> accessLock(_accessIndexMutex);
  _indexData = indexes;
  _indexBuffer->setData(_indexData, commandBufferTransfer);
}

const std::vector<uint32_t>& MeshStatic3D::getIndexData() {
  std::unique_lock<std::mutex> accessLock(_accessIndexMutex);
  return _indexData;
}

const std::vector<Vertex3D>& MeshStatic3D::getVertexData() {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  return _vertexData;
}

std::shared_ptr<VertexBuffer<Vertex3D>> MeshStatic3D::getVertexBuffer() {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  return _vertexBuffer;
}

std::shared_ptr<VertexBuffer<uint32_t>> MeshStatic3D::getIndexBuffer() {
  std::unique_lock<std::mutex> accessLock(_accessIndexMutex);
  return _indexBuffer;
}

void MeshStatic3D::setColor(std::vector<glm::vec3> color, std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  if (color.size() == 0) return;
  if (color.size() > 1 && color.size() != _vertexData.size())
    throw std::invalid_argument("Number of colors doesn't match number of vertices");

  glm::vec3 colorValue = color.front();
  for (int i = 0; i < _vertexData.size(); i++) {
    if (i < color.size()) colorValue = color[i];
    _vertexData[i].color = colorValue;
  }
  _vertexBuffer->setData(_vertexData, commandBufferTransfer);
}

void MeshStatic3D::setNormal(std::vector<glm::vec3> normal, std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  if (normal.size() == 0) return;
  if (normal.size() > 1 && normal.size() != _vertexData.size())
    throw std::invalid_argument("Number of normals doesn't match number of vertices");

  glm::vec3 normalValue = normal.front();
  for (int i = 0; i < _vertexData.size(); i++) {
    if (i < normal.size()) normalValue = normal[i];
    _vertexData[i].normal = normalValue;
  }
  _vertexBuffer->setData(_vertexData, commandBufferTransfer);
}

void MeshStatic3D::setPosition(std::vector<glm::vec3> position, std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  if (position.size() == 0) return;
  if (position.size() > 1 && position.size() != _vertexData.size())
    throw std::invalid_argument("Number of positions doesn't match number of vertices");

  glm::vec3 positionValue = position.front();
  for (int i = 0; i < _vertexData.size(); i++) {
    if (i < position.size()) positionValue = position[i];
    _vertexData[i].pos = positionValue;
  }
  _vertexBuffer->setData(_vertexData, commandBufferTransfer);
}

void MeshStatic3D::setAABB(std::shared_ptr<AABB> aabb) { _aabb = aabb; }

void MeshStatic3D::addPrimitive(MeshPrimitive primitive) { _primitives.push_back(primitive); }

const std::vector<MeshPrimitive>& MeshStatic3D::getPrimitives() { return _primitives; }

std::shared_ptr<AABB> MeshStatic3D::getAABB() { return _aabb; }

VkVertexInputBindingDescription MeshStatic3D::getBindingDescription() {
  VkVertexInputBindingDescription bindingDescription{};
  bindingDescription.binding = 0;
  bindingDescription.stride = sizeof(Vertex3D);
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> MeshStatic3D::getAttributeDescriptions() {
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

MeshDynamic3D::MeshDynamic3D(std::shared_ptr<EngineState> engineState) : Mesh(engineState) {
  _vertexBuffer = std::make_shared<VertexBufferDynamic<Vertex3D>>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, _engineState);
  _indexBuffer = std::make_shared<VertexBufferDynamic<uint32_t>>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, _engineState);
}

void MeshDynamic3D::setVertices(std::vector<Vertex3D> vertices) {
  _vertexData = vertices;
  _vertexBuffer->setData(_vertexData);
}

void MeshDynamic3D::setIndexes(std::vector<uint32_t> indexes) {
  _indexData = indexes;
  _indexBuffer->setData(_indexData);
}

const std::vector<uint32_t>& MeshDynamic3D::getIndexData() { return _indexData; }

const std::vector<Vertex3D>& MeshDynamic3D::getVertexData() { return _vertexData; }

std::shared_ptr<VertexBuffer<Vertex3D>> MeshDynamic3D::getVertexBuffer() { return _vertexBuffer; }

std::shared_ptr<VertexBuffer<uint32_t>> MeshDynamic3D::getIndexBuffer() { return _indexBuffer; }

void MeshDynamic3D::setColor(std::vector<glm::vec3> color) {
  if (color.size() == 0) return;
  if (color.size() > 1 && color.size() != _vertexData.size())
    throw std::invalid_argument("Number of colors doesn't match number of vertices");

  glm::vec3 colorValue = color.front();
  for (int i = 0; i < _vertexData.size(); i++) {
    if (i < color.size()) colorValue = color[i];
    _vertexData[i].color = colorValue;
  }
  _vertexBuffer->setData(_vertexData);
}

void MeshDynamic3D::setNormal(std::vector<glm::vec3> normal) {
  if (normal.size() == 0) return;
  if (normal.size() > 1 && normal.size() != _vertexData.size())
    throw std::invalid_argument("Number of normals doesn't match number of vertices");

  glm::vec3 normalValue = normal.front();
  for (int i = 0; i < _vertexData.size(); i++) {
    if (i < normal.size()) normalValue = normal[i];
    _vertexData[i].normal = normalValue;
  }
  _vertexBuffer->setData(_vertexData);
}

void MeshDynamic3D::setPosition(std::vector<glm::vec3> position) {
  if (position.size() == 0) return;
  if (position.size() > 1 && position.size() != _vertexData.size())
    throw std::invalid_argument("Number of positions doesn't match number of vertices");

  glm::vec3 positionValue = position.front();
  for (int i = 0; i < _vertexData.size(); i++) {
    if (i < position.size()) positionValue = position[i];
    _vertexData[i].pos = positionValue;
  }
  _vertexBuffer->setData(_vertexData);
}

void MeshDynamic3D::setAABB(std::shared_ptr<AABB> aabb) { _aabb = aabb; }

void MeshDynamic3D::addPrimitive(MeshPrimitive primitive) { _primitives.push_back(primitive); }

const std::vector<MeshPrimitive>& MeshDynamic3D::getPrimitives() { return _primitives; }

std::shared_ptr<AABB> MeshDynamic3D::getAABB() { return _aabb; }

VkVertexInputBindingDescription MeshDynamic3D::getBindingDescription() {
  VkVertexInputBindingDescription bindingDescription{};
  bindingDescription.binding = 0;
  bindingDescription.stride = sizeof(Vertex3D);
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> MeshDynamic3D::getAttributeDescriptions() {
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

MeshBoundingBox::MeshBoundingBox(glm::vec3 minPoint,
                                 glm::vec3 maxPoint,
                                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                                 std::shared_ptr<EngineState> engineState)
    : MeshStatic3D(engineState) {
  std::vector<Vertex3D> vertices(24);
  // normal - z, tangent - y, bitangent - x
  // 0
  vertices[0].pos = glm::vec3(minPoint.x, minPoint.y, maxPoint.z);
  vertices[0].normal = glm::vec3(0.0, -1.0, 0.0);  // down
  vertices[0].tangent = glm::vec4(0.0, 0.0, 1.0, 1.0);
  vertices[1].pos = glm::vec3(minPoint.x, minPoint.y, maxPoint.z);
  vertices[1].normal = glm::vec3(0.0, 0.0, 1.0);  // front
  vertices[1].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  vertices[2].pos = glm::vec3(minPoint.x, minPoint.y, maxPoint.z);
  vertices[2].normal = glm::vec3(-1.0, 0.0, 0.0);  // left
  vertices[2].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  // 1
  vertices[3].pos = glm::vec3(maxPoint.x, minPoint.y, maxPoint.z);
  vertices[3].normal = glm::vec3(0.0, -1.0, 0.0);  // down
  vertices[3].tangent = glm::vec4(0.0, 0.0, 1.0, 1.0);
  vertices[4].pos = glm::vec3(maxPoint.x, minPoint.y, maxPoint.z);
  vertices[4].normal = glm::vec3(0.0, 0.0, 1.0);  // front
  vertices[4].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  vertices[5].pos = glm::vec3(maxPoint.x, minPoint.y, maxPoint.z);
  vertices[5].normal = glm::vec3(1.0, 0.0, 0.0);  // right
  vertices[5].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  // 2
  vertices[6].pos = glm::vec3(minPoint.x, maxPoint.y, maxPoint.z);
  vertices[6].normal = glm::vec3(0.0, 1.0, 0.0);  // top
  vertices[6].tangent = glm::vec4(0.0, 0.0, -1.0, 1.0);
  vertices[7].pos = glm::vec3(minPoint.x, maxPoint.y, maxPoint.z);
  vertices[7].normal = glm::vec3(0.0, 0.0, 1.0);  // front
  vertices[7].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  vertices[8].pos = glm::vec3(minPoint.x, maxPoint.y, maxPoint.z);
  vertices[8].normal = glm::vec3(-1.0, 0.0, 0.0);  // left
  vertices[8].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  // 3
  vertices[9].pos = glm::vec3(maxPoint.x, maxPoint.y, maxPoint.z);
  vertices[9].normal = glm::vec3(0.0, 1.0, 0.0);  // top
  vertices[9].tangent = glm::vec4(0.0, 0.0, -1.0, 1.0);
  vertices[10].pos = glm::vec3(maxPoint.x, maxPoint.y, maxPoint.z);
  vertices[10].normal = glm::vec3(0.0, 0.0, 1.0);  // front
  vertices[10].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  vertices[11].pos = glm::vec3(maxPoint.x, maxPoint.y, maxPoint.z);
  vertices[11].normal = glm::vec3(1.0, 0.0, 0.0);  // right
  vertices[11].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  // 4
  vertices[12].pos = glm::vec3(minPoint.x, minPoint.y, minPoint.z);
  vertices[12].normal = glm::vec3(0.0, -1.0, 0.0);  // down
  vertices[12].tangent = glm::vec4(0.0, 0.0, 1.0, 1.0);
  vertices[13].pos = glm::vec3(minPoint.x, minPoint.y, minPoint.z);
  vertices[13].normal = glm::vec3(0.0, 0.0, -1.0);  // back
  vertices[13].tangent = glm::vec4(0.0, -1.0, 0.0, 1.0);
  vertices[14].pos = glm::vec3(minPoint.x, minPoint.y, minPoint.z);
  vertices[14].normal = glm::vec3(-1.0, 0.0, 0.0);  // left
  vertices[14].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  // 5
  vertices[15].pos = glm::vec3(maxPoint.x, minPoint.y, minPoint.z);
  vertices[15].normal = glm::vec3(0.0, -1.0, 0.0);  // down
  vertices[15].tangent = glm::vec4(0.0, 0.0, 1.0, 1.0);
  vertices[16].pos = glm::vec3(maxPoint.x, minPoint.y, minPoint.z);
  vertices[16].normal = glm::vec3(0.0, 0.0, -1.0);  // back
  vertices[16].tangent = glm::vec4(0.0, -1.0, 0.0, 1.0);
  vertices[17].pos = glm::vec3(maxPoint.x, minPoint.y, minPoint.z);
  vertices[17].normal = glm::vec3(1.0, 0.0, 0.0);  // right
  vertices[17].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  // 6
  vertices[18].pos = glm::vec3(minPoint.x, maxPoint.y, minPoint.z);
  vertices[18].normal = glm::vec3(0.0, 1.0, 0.0);  // top
  vertices[18].tangent = glm::vec4(0.0, 0.0, -1.0, 1.0);
  vertices[19].pos = glm::vec3(minPoint.x, maxPoint.y, minPoint.z);
  vertices[19].normal = glm::vec3(0.0, 0.0, -1.0);  // back
  vertices[19].tangent = glm::vec4(0.0, -1.0, 0.0, 1.0);
  vertices[20].pos = glm::vec3(minPoint.x, maxPoint.y, minPoint.z);
  vertices[20].normal = glm::vec3(-1.0, 0.0, 0.0);  // left
  vertices[20].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  // 7
  vertices[21].pos = glm::vec3(maxPoint.x, maxPoint.y, minPoint.z);
  vertices[21].normal = glm::vec3(0.0, 1.0, 0.0);  // top
  vertices[21].tangent = glm::vec4(0.0, 0.0, -1.0, 1.0);
  vertices[22].pos = glm::vec3(maxPoint.x, maxPoint.y, minPoint.z);
  vertices[22].normal = glm::vec3(0.0, 0.0, -1.0);  // back
  vertices[22].tangent = glm::vec4(0.0, -1.0, 0.0, 1.0);
  vertices[23].pos = glm::vec3(maxPoint.x, maxPoint.y, minPoint.z);
  vertices[23].normal = glm::vec3(1.0, 0.0, 0.0);  // right
  vertices[23].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);

  std::vector<uint32_t> indices{                          // Bottom
                                0, 12, 15, 15, 3, 0,      // ccw if look to this face from down
                                                          //  Top
                                6, 9, 21, 21, 18, 6,      // ccw if look to this face from up
                                                          //  Left
                                2, 8, 20, 20, 14, 2,      // ccw if look to this face from left
                                                          //  Right
                                5, 17, 23, 23, 11, 5,     // ccw if look to this face from right
                                                          //  Front
                                4, 10, 7, 7, 1, 4,        // ccw if look to this face from front
                                                          //  Back
                                16, 13, 19, 19, 22, 16};  // ccw if look to this face from back
  setVertices(vertices, commandBufferTransfer);
  setIndexes(indices, commandBufferTransfer);
  setColor(std::vector<glm::vec3>(vertices.size(), glm::vec3(1.f, 1.f, 1.f)), commandBufferTransfer);
}

MeshCube::MeshCube(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<EngineState> engineState)
    : MeshStatic3D(engineState) {
  std::vector<Vertex3D> vertices(24);
  // normal - z, tangent - y, bitangent - x
  // 0
  vertices[0].pos = glm::vec3(-0.5, -0.5, 0.5);
  vertices[0].normal = glm::vec3(0.0, -1.0, 0.0);  // down
  vertices[0].tangent = glm::vec4(0.0, 0.0, 1.0, 1.0);
  vertices[1].pos = glm::vec3(-0.5, -0.5, 0.5);
  vertices[1].normal = glm::vec3(0.0, 0.0, 1.0);  // front
  vertices[1].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  vertices[2].pos = glm::vec3(-0.5, -0.5, 0.5);
  vertices[2].normal = glm::vec3(-1.0, 0.0, 0.0);  // left
  vertices[2].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  // 1
  vertices[3].pos = glm::vec3(0.5, -0.5, 0.5);
  vertices[3].normal = glm::vec3(0.0, -1.0, 0.0);  // down
  vertices[3].tangent = glm::vec4(0.0, 0.0, 1.0, 1.0);
  vertices[4].pos = glm::vec3(0.5, -0.5, 0.5);
  vertices[4].normal = glm::vec3(0.0, 0.0, 1.0);  // front
  vertices[4].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  vertices[5].pos = glm::vec3(0.5, -0.5, 0.5);
  vertices[5].normal = glm::vec3(1.0, 0.0, 0.0);  // right
  vertices[5].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  // 2
  vertices[6].pos = glm::vec3(-0.5, 0.5, 0.5);
  vertices[6].normal = glm::vec3(0.0, 1.0, 0.0);  // top
  vertices[6].tangent = glm::vec4(0.0, 0.0, -1.0, 1.0);
  vertices[7].pos = glm::vec3(-0.5, 0.5, 0.5);
  vertices[7].normal = glm::vec3(0.0, 0.0, 1.0);  // front
  vertices[7].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  vertices[8].pos = glm::vec3(-0.5, 0.5, 0.5);
  vertices[8].normal = glm::vec3(-1.0, 0.0, 0.0);  // left
  vertices[8].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  // 3
  vertices[9].pos = glm::vec3(0.5, 0.5, 0.5);
  vertices[9].normal = glm::vec3(0.0, 1.0, 0.0);  // top
  vertices[9].tangent = glm::vec4(0.0, 0.0, -1.0, 1.0);
  vertices[10].pos = glm::vec3(0.5, 0.5, 0.5);
  vertices[10].normal = glm::vec3(0.0, 0.0, 1.0);  // front
  vertices[10].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  vertices[11].pos = glm::vec3(0.5, 0.5, 0.5);
  vertices[11].normal = glm::vec3(1.0, 0.0, 0.0);  // right
  vertices[11].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  // 4
  vertices[12].pos = glm::vec3(-0.5, -0.5, -0.5);
  vertices[12].normal = glm::vec3(0.0, -1.0, 0.0);  // down
  vertices[12].tangent = glm::vec4(0.0, 0.0, 1.0, 1.0);
  vertices[13].pos = glm::vec3(-0.5, -0.5, -0.5);
  vertices[13].normal = glm::vec3(0.0, 0.0, -1.0);  // back
  vertices[13].tangent = glm::vec4(0.0, -1.0, 0.0, 1.0);
  vertices[14].pos = glm::vec3(-0.5, -0.5, -0.5);
  vertices[14].normal = glm::vec3(-1.0, 0.0, 0.0);  // left
  vertices[14].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  // 5
  vertices[15].pos = glm::vec3(0.5, -0.5, -0.5);
  vertices[15].normal = glm::vec3(0.0, -1.0, 0.0);  // down
  vertices[15].tangent = glm::vec4(0.0, 0.0, 1.0, 1.0);
  vertices[16].pos = glm::vec3(0.5, -0.5, -0.5);
  vertices[16].normal = glm::vec3(0.0, 0.0, -1.0);  // back
  vertices[16].tangent = glm::vec4(0.0, -1.0, 0.0, 1.0);
  vertices[17].pos = glm::vec3(0.5, -0.5, -0.5);
  vertices[17].normal = glm::vec3(1.0, 0.0, 0.0);  // right
  vertices[17].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  // 6
  vertices[18].pos = glm::vec3(-0.5, 0.5, -0.5);
  vertices[18].normal = glm::vec3(0.0, 1.0, 0.0);  // top
  vertices[18].tangent = glm::vec4(0.0, 0.0, -1.0, 1.0);
  vertices[19].pos = glm::vec3(-0.5, 0.5, -0.5);
  vertices[19].normal = glm::vec3(0.0, 0.0, -1.0);  // back
  vertices[19].tangent = glm::vec4(0.0, -1.0, 0.0, 1.0);
  vertices[20].pos = glm::vec3(-0.5, 0.5, -0.5);
  vertices[20].normal = glm::vec3(-1.0, 0.0, 0.0);  // left
  vertices[20].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);
  // 7
  vertices[21].pos = glm::vec3(0.5, 0.5, -0.5);
  vertices[21].normal = glm::vec3(0.0, 1.0, 0.0);  // top
  vertices[21].tangent = glm::vec4(0.0, 0.0, -1.0, 1.0);
  vertices[22].pos = glm::vec3(0.5, 0.5, -0.5);
  vertices[22].normal = glm::vec3(0.0, 0.0, -1.0);  // back
  vertices[22].tangent = glm::vec4(0.0, -1.0, 0.0, 1.0);
  vertices[23].pos = glm::vec3(0.5, 0.5, -0.5);
  vertices[23].normal = glm::vec3(1.0, 0.0, 0.0);  // right
  vertices[23].tangent = glm::vec4(0.0, 1.0, 0.0, 1.0);

  std::vector<uint32_t> indices{                          // Bottom
                                0, 12, 15, 15, 3, 0,      // ccw if look to this face from down
                                                          //  Top
                                6, 9, 21, 21, 18, 6,      // ccw if look to this face from up
                                                          //  Left
                                2, 8, 20, 20, 14, 2,      // ccw if look to this face from left
                                                          //  Right
                                5, 17, 23, 23, 11, 5,     // ccw if look to this face from right
                                                          //  Front
                                4, 10, 7, 7, 1, 4,        // ccw if look to this face from front
                                                          //  Back
                                16, 13, 19, 19, 22, 16};  // ccw if look to this face from back
  setVertices(vertices, commandBufferTransfer);
  setIndexes(indices, commandBufferTransfer);
  setColor(std::vector<glm::vec3>(vertices.size(), glm::vec3(1.f, 1.f, 1.f)), commandBufferTransfer);
}

MeshSphere::MeshSphere(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<EngineState> engineState)
    : MeshStatic3D(engineState) {
  int radius = 1;
  int sectorCount = 20;
  int stackCount = 20;

  float x, y, z, xz;                            // vertex position
  float nx, ny, nz, lengthInv = 1.0f / radius;  // vertex normal
  float s, t;                                   // vertex texCoord

  float sectorStep = 2 * M_PI / sectorCount;
  float stackStep = M_PI / stackCount;
  float sectorAngle, stackAngle;

  std::vector<Vertex3D> vertices;
  for (int i = 0; i <= stackCount; ++i) {
    Vertex3D vertex;
    stackAngle = M_PI / 2 - i * stackStep;  // starting from pi/2 to -pi/2
    xz = radius * cosf(stackAngle);         // r * cos(u)
    y = radius * sinf(stackAngle);          // r * sin(u)

    // add (sectorCount+1) vertices per stack
    // first and last vertices have same position and normal, but different tex coords
    for (int j = 0; j <= sectorCount; ++j) {
      sectorAngle = 2 * M_PI - j * sectorStep;  // starting from 0 to 2pi

      // vertex position (x, y, z)
      x = xz * cosf(sectorAngle);  // r * cos(u) * cos(v)
      z = xz * sinf(sectorAngle);  // r * cos(u) * sin(v)
      vertex.pos = glm::vec3(x, y, z);

      // normalized vertex normal (nx, ny, nz)
      nx = x * lengthInv;
      ny = y * lengthInv;
      nz = z * lengthInv;
      vertex.normal = glm::vec3(nx, ny, nz);

      // calculate tangent as dp/du
      // (https://computergraphics.stackexchange.com/questions/5498/compute-sphere-tangent-for-normal-mapping)
      vertex.tangent = glm::vec4(-sinf(sectorAngle), 0.f, cosf(sectorAngle), 1.f);

      // vertex tex coord (s, t) range between [0, 1]
      s = (float)j / sectorCount;
      t = (float)i / stackCount;
      vertex.texCoord = glm::vec2(s, t);
      vertices.push_back(vertex);
    }
  }

  std::vector<uint32_t> indexes;
  int k1, k2;
  for (int i = 0; i < stackCount; ++i) {
    k1 = i * (sectorCount + 1);  // beginning of current stack
    k2 = k1 + sectorCount + 1;   // beginning of next stack

    for (int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
      // 2 triangles per sector excluding first and last stacks
      // k1 => k2 => k1+1
      if (i != 0) {
        indexes.push_back(k1);
        indexes.push_back(k2);
        indexes.push_back(k1 + 1);
      }

      // k1+1 => k2 => k2+1
      if (i != (stackCount - 1)) {
        indexes.push_back(k1 + 1);
        indexes.push_back(k2);
        indexes.push_back(k2 + 1);
      }
    }
  }
  setVertices(vertices, commandBufferTransfer);
  setIndexes(indexes, commandBufferTransfer);
  setColor(std::vector<glm::vec3>(vertices.size(), glm::vec3(1.f, 1.f, 1.f)), commandBufferTransfer);
}

MeshStatic2D::MeshStatic2D(std::shared_ptr<EngineState> engineState) : Mesh(engineState) {
  _vertexBuffer = std::make_shared<VertexBufferStatic<Vertex2D>>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, _engineState);
  _indexBuffer = std::make_shared<VertexBufferStatic<uint32_t>>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, _engineState);
}

void MeshStatic2D::setVertices(std::vector<Vertex2D> vertices, std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  _vertexData = vertices;
  _vertexBuffer->setData(vertices, commandBufferTransfer);
}

void MeshStatic2D::setIndexes(std::vector<uint32_t> indexes, std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  std::unique_lock<std::mutex> accessLock(_accessIndexMutex);
  _indexData = indexes;
  _indexBuffer->setData(indexes, commandBufferTransfer);
}

void MeshStatic2D::setColor(glm::vec3 color, std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  for (auto& vertex : _vertexData) {
    vertex.color = color;
  }
  _vertexBuffer->setData(_vertexData, commandBufferTransfer);
}

void MeshStatic2D::setNormal(glm::vec3 normal, std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  for (auto& vertex : _vertexData) {
    vertex.normal = normal;
  }
  _vertexBuffer->setData(_vertexData, commandBufferTransfer);
}

const std::vector<uint32_t>& MeshStatic2D::getIndexData() {
  std::unique_lock<std::mutex> accessLock(_accessIndexMutex);
  return _indexData;
}

const std::vector<Vertex2D>& MeshStatic2D::getVertexData() {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  return _vertexData;
}

std::shared_ptr<VertexBuffer<Vertex2D>> MeshStatic2D::getVertexBuffer() {
  std::unique_lock<std::mutex> accessLock(_accessVertexMutex);
  return _vertexBuffer;
}

std::shared_ptr<VertexBuffer<uint32_t>> MeshStatic2D::getIndexBuffer() {
  std::unique_lock<std::mutex> accessLock(_accessIndexMutex);
  return _indexBuffer;
}

VkVertexInputBindingDescription MeshStatic2D::getBindingDescription() {
  VkVertexInputBindingDescription bindingDescription{};
  bindingDescription.binding = 0;
  bindingDescription.stride = sizeof(Vertex2D);
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> MeshStatic2D::getAttributeDescriptions() {
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
  attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
  attributeDescriptions[4].offset = offsetof(Vertex2D, tangent);

  return attributeDescriptions;
}