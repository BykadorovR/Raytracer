#define TINYOBJLOADER_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "Model.h"
#include <unordered_map>

struct UniformObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 projection;
};

void ModelOBJ::_loadModel() {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, _path.c_str())) {
    throw std::runtime_error(warn + err);
  }

  std::unordered_map<Vertex3D, uint32_t> uniqueVertices{};
  for (const auto& shape : shapes) {
    for (const auto& index : shape.mesh.indices) {
      Vertex3D vertex{};
      vertex.pos = {attrib.vertices[3 * index.vertex_index + 0], attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]};

      vertex.texCoord = {attrib.texcoords[2 * index.texcoord_index + 0],
                         attrib.texcoords[2 * index.texcoord_index + 1]};

      vertex.color = {1.0f, 1.0f, 1.0f};
      vertex.normal = {attrib.normals[3 * index.normal_index + 0], attrib.normals[3 * index.normal_index + 1],
                       attrib.normals[3 * index.normal_index + 2]};

      if (uniqueVertices.count(vertex) == 0) {
        uniqueVertices[vertex] = static_cast<uint32_t>(_vertices.size());
        _vertices.push_back(vertex);
      }
      _indices.push_back(uniqueVertices[vertex]);
    }
  }
}

ModelOBJ::ModelOBJ(std::string path,
                   std::shared_ptr<Texture> texture,
                   std::shared_ptr<DescriptorSetLayout> descriptorSetLayout,
                   std::shared_ptr<Pipeline> pipeline,
                   std::shared_ptr<DescriptorPool> descriptorPool,
                   std::shared_ptr<CommandPool> commandPool,
                   std::shared_ptr<CommandBuffer> commandBuffer,
                   std::shared_ptr<Queue> queue,
                   std::shared_ptr<Device> device,
                   std::shared_ptr<Settings> settings) {
  _path = path;
  _pipeline = pipeline;
  _commandBuffer = commandBuffer;
  _device = device;
  _settings = settings;
  _texture = texture;

  _loadModel();
  _vertexBuffer = std::make_shared<VertexBuffer3D>(_vertices, commandPool, queue, device);
  _indexBuffer = std::make_shared<IndexBuffer>(_indices, commandPool, queue, device);
  _uniformBuffer = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(UniformObject), commandPool,
                                                   queue, device);
  _descriptorSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), descriptorSetLayout,
                                                   descriptorPool, device);
  _descriptorSet->createGraphic(texture, _uniformBuffer);

  _model = glm::mat4(1.f);
  _view = glm::mat4(1.f);
  _projection = glm::mat4(1.f);
}

void ModelOBJ::setModel(glm::mat4 model) { _model = model; }

void ModelOBJ::setView(glm::mat4 view) { _view = view; }

void ModelOBJ::setProjection(glm::mat4 projection) { _projection = projection; }

void ModelOBJ::draw(int currentFrame) {
  UniformObject ubo{};
  ubo.model = _model;
  ubo.view = _view;
  ubo.projection = _projection;

  void* data;
  vkMapMemory(_device->getLogicalDevice(), _uniformBuffer->getBuffer()[currentFrame]->getMemory(), 0, sizeof(ubo), 0,
              &data);
  memcpy(data, &ubo, sizeof(ubo));
  vkUnmapMemory(_device->getLogicalDevice(), _uniformBuffer->getBuffer()[currentFrame]->getMemory());

  VkBuffer vertexBuffers[] = {_vertexBuffer->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(_commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(_commandBuffer->getCommandBuffer()[currentFrame], _indexBuffer->getBuffer()->getData(), 0,
                       VK_INDEX_TYPE_UINT32);
  vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                          _pipeline->getPipelineLayout(), 0, 1, &_descriptorSet->getDescriptorSets()[currentFrame], 0,
                          nullptr);

  vkCmdDrawIndexed(_commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_indices.size()), 1, 0, 0,
                   0);
}

ModelGLTF::ModelGLTF(std::string path) {
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;
  std::vector<uint32_t> indexBuffer;
  std::vector<Vertex3D> vertexBuffer;
  bool loaded = loader.LoadASCIIFromFile(&model, &err, &warn, path);
  if (loaded) {
    _loadImages(model);
    _loadMaterials(model);
    _loadTextures(model);
    const tinygltf::Scene& scene = model.scenes[0];
    for (size_t i = 0; i < scene.nodes.size(); i++) {
      tinygltf::Node& node = model.nodes[scene.nodes[i]];
      _loadNode(node, model, indexBuffer, vertexBuffer);
    }
  }
}

void ModelGLTF::_loadImages(tinygltf::Model& model) {}

void ModelGLTF::_loadMaterials(tinygltf::Model& model) {}

void ModelGLTF::_loadTextures(tinygltf::Model& model) {}

void ModelGLTF::_loadNode(tinygltf::Node& node,
                          tinygltf::Model& model,
                          std::vector<uint32_t>& indexBuffer,
                          std::vector<Vertex3D>& vertexBuffer) {}

void ModelGLTF::draw(int currentFrame) {}