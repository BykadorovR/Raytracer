#define TINYOBJLOADER_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "Model.h"
#include <unordered_map>
#include "glm/gtc/type_ptr.hpp"

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
      vertex.pos = {attrib.vertices[3 * index.vertex_index + 0], 
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]};

      vertex.texCoord = {attrib.texcoords[2 * index.texcoord_index + 0],
                         1.f - attrib.texcoords[2 * index.texcoord_index + 1]};

      vertex.color = {1.0f, 1.0f, 1.0f};
      vertex.normal = {attrib.normals[3 * index.normal_index + 0], 
                       attrib.normals[3 * index.normal_index + 1],
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
  _uniformBufferCamera = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(UniformObjectCamera), commandPool,
                                                   queue, device);
  _uniformBufferLights = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(UniformObjectLights),
                                                   commandPool, queue, device);
  _descriptorSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), descriptorSetLayout,
                                                   descriptorPool, device);
  _descriptorSet->createGraphic(texture, _uniformBufferCamera, _uniformBufferLights);

  _model = glm::mat4(1.f);
  _view = glm::mat4(1.f);
  _projection = glm::mat4(1.f);
}

void ModelOBJ::setModel(glm::mat4 model) { _model = model; }

void ModelOBJ::setView(glm::mat4 view) { _view = view; }

void ModelOBJ::setProjection(glm::mat4 projection) { _projection = projection; }

void ModelOBJ::setPosition(glm::vec3 position) { _position = position; }

void ModelOBJ::draw(int currentFrame) {
  {
    UniformObjectCamera ubo{};
    ubo.model = _model;
    ubo.view = _view;
    ubo.projection = _projection;
    ubo.position = _position;

    void* data;
    vkMapMemory(_device->getLogicalDevice(), _uniformBufferCamera->getBuffer()[currentFrame]->getMemory(), 0, sizeof(ubo), 0,
                &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(_device->getLogicalDevice(), _uniformBufferCamera->getBuffer()[currentFrame]->getMemory());
  }
  {
    std::array<UniformObjectPointLight, 3> lights{};
    lights[0].position = glm::vec3(1, 1, 1);
    lights[0].color = glm::vec3(1., 0.8f, 0.5f);
    lights[0].radius = 1.6f;
    lights[1].position = glm::vec3(0, -1, 0.5f);
    lights[1].color = glm::vec3(0.2f, 1, 1);
    lights[1].radius = 1;
    lights[2].position = glm::vec3(0.15f, 0.97f, 0.2f);
    lights[2].color = glm::vec3(2, 0.5f, 0);
    lights[2].radius = 0.8f;

    UniformObjectLights ubo{};
    ubo.number = 3;
    for (int i = 0; i < ubo.number; i++) {
      ubo.pLights[i] = lights[i];
    }
    void* data;
    vkMapMemory(_device->getLogicalDevice(), _uniformBufferLights->getBuffer()[currentFrame]->getMemory(), 0,
                sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(_device->getLogicalDevice(), _uniformBufferLights->getBuffer()[currentFrame]->getMemory());
  }

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

ModelGLTF::ModelGLTF(std::string path,
                     std::shared_ptr<DescriptorSetLayout> descriptorSetLayout,
                     std::shared_ptr<Pipeline> pipeline,
                     std::shared_ptr<DescriptorPool> descriptorPool,
                     std::shared_ptr<CommandPool> commandPool,
                     std::shared_ptr<CommandBuffer> commandBuffer,
                     std::shared_ptr<Queue> queue,
                     std::shared_ptr<Device> device,
                     std::shared_ptr<Settings> settings) {
  _device = device;
  _queue = queue;
  _commandPool = commandPool;
  _pipeline = pipeline;
  _descriptorPool = descriptorPool;
  _commandBuffer = commandBuffer;

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
      _loadNode(node, model, nullptr, indexBuffer, vertexBuffer);
    }
  }

  _vertexBuffer = std::make_shared<VertexBuffer3D>(vertexBuffer, commandPool, queue, device);
  _indexBuffer = std::make_shared<IndexBuffer>(indexBuffer, commandPool, queue, device);
  _uniformBufferCamera = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(UniformObjectCamera),
                                                         commandPool, queue, device);
  _uniformBufferLights = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(UniformObjectLights),
                                                         commandPool, queue, device);
  for (auto& image : _images) {
    image.descriptorSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), descriptorSetLayout,
                                                          _descriptorPool, _device);
    image.descriptorSet->createGraphic(image.texture, _uniformBufferCamera, _uniformBufferLights);
  }
}

void ModelGLTF::_loadImages(tinygltf::Model& model) {
  _images.resize(model.images.size());
  for (int i = 0; i < model.images.size(); i++) {
    tinygltf::Image& glTFImage = model.images[i];
    // Get the image data from the glTF loader
    unsigned char* buffer = nullptr;
    int bufferSize = 0;
    bool deleteBuffer = false;
    // We convert RGB-only images to RGBA, as most devices don't support RGB-formats in Vulkan
    if (glTFImage.component == 3) {
      bufferSize = glTFImage.width * glTFImage.height * 4;
      buffer = new unsigned char[bufferSize];
      unsigned char* rgba = buffer;
      unsigned char* rgb = &glTFImage.image[0];
      for (size_t i = 0; i < glTFImage.width * glTFImage.height; ++i) {
        memcpy(rgba, rgb, sizeof(unsigned char) * 3);
        rgba += 4;
        rgb += 3;
      }
      deleteBuffer = true;
    } else {
      buffer = &glTFImage.image[0];
      bufferSize = glTFImage.image.size();
    }

    // copy buffer to Texture
    auto stagingBuffer = std::make_shared<Buffer>(
        bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _device);

    void* data;
    vkMapMemory(_device->getLogicalDevice(), stagingBuffer->getMemory(), 0, bufferSize, 0, &data);
    memcpy(data, buffer, bufferSize);
    vkUnmapMemory(_device->getLogicalDevice(), stagingBuffer->getMemory());

    auto image = std::make_shared<Image>(
        std::tuple{glTFImage.width, glTFImage.height}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _device);

    image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _commandPool, _queue);
    image->copyFrom(stagingBuffer, _commandPool, _queue);
    image->changeLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, _commandPool, _queue);

    auto imageView = std::make_shared<ImageView>(image, VK_IMAGE_ASPECT_COLOR_BIT, _device);
    auto texture = std::make_shared<Texture>(imageView, _device);

    _images[i].texture = texture;

    if (deleteBuffer) {
      delete[] buffer;
    }
  }
}

void ModelGLTF::_loadMaterials(tinygltf::Model& model) {
  _materials.resize(model.materials.size());
  for (size_t i = 0; i < model.materials.size(); i++) {
    // We only read the most basic properties required for our sample
    tinygltf::Material glTFMaterial = model.materials[i];
    // Get the base color factor
    if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end()) {
      _materials[i].baseColorFactor = glm::make_vec4(glTFMaterial.values["baseColorFactor"].ColorFactor().data());
    }
    // Get base color texture index
    if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end()) {
      _materials[i].baseColorTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
    }
  }
}

void ModelGLTF::_loadTextures(tinygltf::Model& model) {
  _textures.resize(model.textures.size());
  for (int i = 0; i < model.textures.size(); i++) {
    _textures[i].imageIndex = model.textures[i].source;
  }
}

void ModelGLTF::_loadNode(tinygltf::Node& input,
                          tinygltf::Model& model,
                          NodeGLTF* parent,
                          std::vector<uint32_t>& indexBuffer,
                          std::vector<Vertex3D>& vertexBuffer) {
  NodeGLTF* node = new NodeGLTF();
  node->parent = parent;
  node->matrix = glm::mat4(1.f);

  // Get the local node matrix
  // It's either made up from translation, rotation, scale or a 4x4 matrix
  if (input.translation.size() == 3) {
    node->matrix = glm::translate(node->matrix, glm::vec3(glm::make_vec3(input.translation.data())));
  }

  if (input.rotation.size() == 4) {
    glm::quat q = glm::make_quat(input.rotation.data());
    node->matrix *= glm::mat4(q);
  }

  if (input.scale.size() == 3) {
    node->matrix = glm::scale(node->matrix, glm::vec3(glm::make_vec3(input.scale.data())));
  }

  if (input.matrix.size() == 16) {
    node->matrix = glm::make_mat4x4(input.matrix.data());
  };

  // Load node's children
  if (input.children.size() > 0) {
    for (size_t i = 0; i < input.children.size(); i++) {
      _loadNode(model.nodes[input.children[i]], model, node, indexBuffer, vertexBuffer);
    }
  }

  // If the node contains mesh data, we load vertices and indices from the buffers
  // In glTF this is done via accessors and buffer views
  if (input.mesh > -1) {
    const tinygltf::Mesh mesh = model.meshes[input.mesh];
    // Iterate through all primitives of this node's mesh
    for (size_t i = 0; i < mesh.primitives.size(); i++) {
      const tinygltf::Primitive& glTFPrimitive = mesh.primitives[i];
      uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.size());
      uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
      uint32_t indexCount = 0;
      // Vertices
      {
        const float* positionBuffer = nullptr;
        const float* normalsBuffer = nullptr;
        const float* texCoordsBuffer = nullptr;
        size_t vertexCount = 0;

        // Get buffer data for vertex positions
        if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
          const tinygltf::Accessor& accessor = model.accessors[glTFPrimitive.attributes.find("POSITION")->second];
          const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
          positionBuffer = reinterpret_cast<const float*>(
              &(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
          vertexCount = accessor.count;
        }
        // Get buffer data for vertex normals
        if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
          const tinygltf::Accessor& accessor = model.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
          const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
          normalsBuffer = reinterpret_cast<const float*>(
              &(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
        }
        // Get buffer data for vertex texture coordinates
        // glTF supports multiple sets, we only load the first one
        if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
          const tinygltf::Accessor& accessor = model.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
          const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
          texCoordsBuffer = reinterpret_cast<const float*>(
              &(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
        }

        // Append data to model's vertex buffer
        for (size_t v = 0; v < vertexCount; v++) {
          Vertex3D vert{};
          vert.pos = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
          vert.normal = glm::normalize(
              glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
          vert.texCoord = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
          vert.color = glm::vec3(1.0f);
          vertexBuffer.push_back(vert);
        }
      }
      // Indices
      {
        const tinygltf::Accessor& accessor = model.accessors[glTFPrimitive.indices];
        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

        indexCount += static_cast<uint32_t>(accessor.count);

        // glTF supports different component types of indices
        switch (accessor.componentType) {
          case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
            const uint32_t* buf = reinterpret_cast<const uint32_t*>(
                &buffer.data[accessor.byteOffset + bufferView.byteOffset]);
            for (size_t index = 0; index < accessor.count; index++) {
              indexBuffer.push_back(buf[index] + vertexStart);
            }
            break;
          }
          case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
            const uint16_t* buf = reinterpret_cast<const uint16_t*>(
                &buffer.data[accessor.byteOffset + bufferView.byteOffset]);
            for (size_t index = 0; index < accessor.count; index++) {
              indexBuffer.push_back(buf[index] + vertexStart);
            }
            break;
          }
          case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
            const uint8_t* buf = reinterpret_cast<const uint8_t*>(
                &buffer.data[accessor.byteOffset + bufferView.byteOffset]);
            for (size_t index = 0; index < accessor.count; index++) {
              indexBuffer.push_back(buf[index] + vertexStart);
            }
            break;
          }
          default:
            std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
            return;
        }
      }
      PrimitiveGLTF primitive{};
      primitive.firstIndex = firstIndex;
      primitive.indexCount = indexCount;
      primitive.materialIndex = glTFPrimitive.material;
      node->mesh.primitives.push_back(primitive);
    }
  }

  if (parent) {
    parent->children.push_back(node);
  } else {
    _nodes.push_back(node);
  }
}

void ModelGLTF::setModel(glm::mat4 model) { _model = model; }

void ModelGLTF::setView(glm::mat4 view) { _view = view; }

void ModelGLTF::setProjection(glm::mat4 projection) { _projection = projection; }

void ModelGLTF::setPosition(glm::vec3 position) { _position = position; }

void ModelGLTF::_drawNode(int currentFrame, NodeGLTF* node) {
  if (node->mesh.primitives.size() > 0) {
    // Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
    glm::mat4 nodeMatrix = node->matrix;
    NodeGLTF* currentParent = node->parent;
    while (currentParent) {
      nodeMatrix = currentParent->matrix * nodeMatrix;
      currentParent = currentParent->parent;
    }
    // pass this matrix to uniforms
    {
      UniformObjectCamera ubo{};
      ubo.model = _model;
      ubo.view = _view;
      ubo.projection = _projection;
      ubo.position = _position;

      void* data;
      vkMapMemory(_device->getLogicalDevice(), _uniformBufferCamera->getBuffer()[currentFrame]->getMemory(), 0,
                  sizeof(ubo), 0, &data);
      memcpy(data, &ubo, sizeof(ubo));
      vkUnmapMemory(_device->getLogicalDevice(), _uniformBufferCamera->getBuffer()[currentFrame]->getMemory());
    }
    {
      std::array<UniformObjectPointLight, 3> lights{};
      lights[0].position = glm::vec3(1, 1, 1);
      lights[0].color = glm::vec3(1., 0.8f, 0.5f);
      lights[0].radius = 1.6f;
      lights[1].position = glm::vec3(0, -1, 0.5f);
      lights[1].color = glm::vec3(0.2f, 1, 1);
      lights[1].radius = 1;
      lights[2].position = glm::vec3(0.15f, 0.97f, 0.2f);
      lights[2].color = glm::vec3(2, 0.5f, 0);
      lights[2].radius = 0.8f;

      UniformObjectLights ubo{};
      ubo.number = 3;
      for (int i = 0; i < ubo.number; i++) {
        ubo.pLights[i] = lights[i];
      }
      void* data;
      vkMapMemory(_device->getLogicalDevice(), _uniformBufferLights->getBuffer()[currentFrame]->getMemory(), 0,
                  sizeof(ubo), 0, &data);
      memcpy(data, &ubo, sizeof(ubo));
      vkUnmapMemory(_device->getLogicalDevice(), _uniformBufferLights->getBuffer()[currentFrame]->getMemory());
    }


    for (PrimitiveGLTF& primitive : node->mesh.primitives) {
      if (primitive.indexCount > 0) {
        // Get the texture index for this primitive
        TextureGLTF texture = _textures[_materials[primitive.materialIndex].baseColorTextureIndex];
        // Bind the descriptor for the current primitive's texture
        vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                _pipeline->getPipelineLayout(), 0, 1,
                                &_images[texture.imageIndex].descriptorSet->getDescriptorSets()[currentFrame], 0,
                                nullptr);
        vkCmdDrawIndexed(_commandBuffer->getCommandBuffer()[currentFrame], primitive.indexCount, 1,
                         primitive.firstIndex, 0, 0);
      }
    }
  }
}

void ModelGLTF::draw(int currentFrame) {
  // All vertices and indices are stored in single buffers, so we only need to bind once
  VkBuffer vertexBuffers[] = {_vertexBuffer->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(_commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);
  vkCmdBindIndexBuffer(_commandBuffer->getCommandBuffer()[currentFrame], _indexBuffer->getBuffer()->getData(), 0,
                       VK_INDEX_TYPE_UINT32);
  // Render all nodes at top-level
  for (auto& node : _nodes) {
    _drawNode(currentFrame, node);
  }
}