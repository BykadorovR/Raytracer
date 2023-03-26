#define TINYOBJLOADER_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "Model.h"
#include <unordered_map>
#include "glm/gtc/type_ptr.hpp"

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
                         1.f - attrib.texcoords[2 * index.texcoord_index + 1]};

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
                   std::shared_ptr<DescriptorSetLayout> layoutCamera,
                   std::shared_ptr<DescriptorSetLayout> layoutGraphic,
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
  _descriptorSetGraphic = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), layoutGraphic,
                                                          descriptorPool, device);
  _descriptorSetGraphic->createGraphic(texture);

  _descriptorSetCamera = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), layoutCamera, descriptorPool,
                                                         device);
  _descriptorSetCamera->createCamera(_uniformBuffer);

  _model = glm::mat4(1.f);
  _view = glm::mat4(1.f);
  _projection = glm::mat4(1.f);
}

void ModelOBJ::setModel(glm::mat4 model) { _model = model; }

void ModelOBJ::setView(glm::mat4 view) { _view = view; }

void ModelOBJ::setProjection(glm::mat4 projection) { _projection = projection; }

void ModelOBJ::draw(float frameTimer, int currentFrame) {
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
                          _pipeline->getPipelineLayout(), 0, 1,
                          &_descriptorSetCamera->getDescriptorSets()[currentFrame], 0, nullptr);
  vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                          _pipeline->getPipelineLayout(), 1, 1,
                          &_descriptorSetGraphic->getDescriptorSets()[currentFrame], 0, nullptr);

  vkCmdDrawIndexed(_commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_indices.size()), 1, 0, 0,
                   0);
}

ModelGLTF::ModelGLTF(std::string path,
                     std::shared_ptr<DescriptorSetLayout> layoutCamera,
                     std::shared_ptr<DescriptorSetLayout> layoutGraphic,
                     std::shared_ptr<DescriptorSetLayout> layoutJoints,
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
    _loadTextures(model);
    _loadMaterials(model);

    const tinygltf::Scene& scene = model.scenes[0];
    for (size_t i = 0; i < scene.nodes.size(); i++) {
      tinygltf::Node& node = model.nodes[scene.nodes[i]];
      _loadNode(node, model, nullptr, scene.nodes[i], indexBuffer, vertexBuffer);
    }

    _loadSkins(model);
    _loadAnimations(model);
    // Calculate initial pose
    for (auto node : _nodes) {
      _updateJoints(node);
    }
  }

  _vertexBuffer = std::make_shared<VertexBuffer3D>(vertexBuffer, commandPool, queue, device);
  _indexBuffer = std::make_shared<IndexBuffer>(indexBuffer, commandPool, queue, device);
  _uniformBuffer = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(UniformObject), commandPool,
                                                   queue, device);

  _descriptorSetCamera = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), layoutCamera, descriptorPool,
                                                         device);
  _descriptorSetCamera->createCamera(_uniformBuffer);

  for (auto& image : _images) {
    image.descriptorSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), layoutGraphic,
                                                          _descriptorPool, _device);
    image.descriptorSet->createGraphic(image.texture);
  }

  _descriptorSetJointsDefault = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), layoutJoints,
                                                                _descriptorPool, _device);
  _defaultSSBO = std::make_shared<Buffer>(sizeof(glm::mat4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                          _device);

  _defaultSSBO->map();
  auto m1 = glm::mat4(1.f);
  // we pass inverse bind matrices to shader via SSBO
  memcpy(_defaultSSBO->getMappedMemory(), &m1, sizeof(glm::mat4));
  _defaultSSBO->unmap();
  _descriptorSetJointsDefault->createJoints(_defaultSSBO);

  // TODO: create descriptor set layout and set for skin->descriptorSet
  for (auto& skin : _skins) {
    skin.descriptorSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), layoutJoints,
                                                         _descriptorPool, _device);
    skin.descriptorSet->createJoints(skin.ssbo);
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

void ModelGLTF::_loadSkins(tinygltf::Model& model) {
  _skins.resize(model.skins.size());
  for (size_t i = 0; i < model.skins.size(); i++) {
    tinygltf::Skin glTFSkin = model.skins[i];
    _skins[i].name = glTFSkin.name;
    // Find the root node of the skeleton
    _skins[i].skeletonRoot = _nodeFromIndex(glTFSkin.skeleton);
    // Find joint nodes
    for (int jointIndex : glTFSkin.joints) {
      NodeGLTF* node = _nodeFromIndex(jointIndex);
      if (node) {
        _skins[i].joints.push_back(node);
      }
    }

    // Get the inverse bind matrices from the buffer associated to this skin
    if (glTFSkin.inverseBindMatrices > -1) {
      const tinygltf::Accessor& accessor = model.accessors[glTFSkin.inverseBindMatrices];
      const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
      const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
      _skins[i].inverseBindMatrices.resize(accessor.count);
      memcpy(_skins[i].inverseBindMatrices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset],
             accessor.count * sizeof(glm::mat4));

      _skins[i].ssbo = std::make_shared<Buffer>(
          sizeof(glm::mat4) * _skins[i].inverseBindMatrices.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _device);
      _skins[i].ssbo->map();
      // we pass inverse bind matrices to shader via SSBO
      memcpy(_skins[i].ssbo->getMappedMemory(), _skins[i].inverseBindMatrices.data(),
             sizeof(glm::mat4) * _skins[i].inverseBindMatrices.size());
    }
  }
}

void ModelGLTF::_loadAnimations(tinygltf::Model& model) {
  _animations.resize(model.animations.size());
  for (size_t i = 0; i < model.animations.size(); i++) {
    tinygltf::Animation glTFAnimation = model.animations[i];
    _animations[i].name = glTFAnimation.name;

    // Samplers
    _animations[i].samplers.resize(glTFAnimation.samplers.size());
    for (size_t j = 0; j < glTFAnimation.samplers.size(); j++) {
      tinygltf::AnimationSampler glTFSampler = glTFAnimation.samplers[j];
      AnimationSamplerGLTF& dstSampler = _animations[i].samplers[j];
      dstSampler.interpolation = glTFSampler.interpolation;

      // Read sampler keyframe input time values
      {
        const tinygltf::Accessor& accessor = model.accessors[glTFSampler.input];
        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
        const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
        const float* buf = static_cast<const float*>(dataPtr);
        for (size_t index = 0; index < accessor.count; index++) {
          dstSampler.inputs.push_back(buf[index]);
        }
        // Adjust animation's start and end times
        for (auto input : _animations[i].samplers[j].inputs) {
          if (input < _animations[i].start) {
            _animations[i].start = input;
          };
          if (input > _animations[i].end) {
            _animations[i].end = input;
          }
        }
      }

      // Read sampler keyframe output translate/rotate/scale values
      {
        const tinygltf::Accessor& accessor = model.accessors[glTFSampler.output];
        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
        const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
        switch (accessor.type) {
          case TINYGLTF_TYPE_VEC3: {
            const glm::vec3* buf = static_cast<const glm::vec3*>(dataPtr);
            for (size_t index = 0; index < accessor.count; index++) {
              dstSampler.outputsVec4.push_back(glm::vec4(buf[index], 0.0f));
            }
            break;
          }
          case TINYGLTF_TYPE_VEC4: {
            const glm::vec4* buf = static_cast<const glm::vec4*>(dataPtr);
            for (size_t index = 0; index < accessor.count; index++) {
              dstSampler.outputsVec4.push_back(buf[index]);
            }
            break;
          }
          default: {
            std::cout << "unknown type" << std::endl;
            break;
          }
        }
      }
    }

    // Channels
    _animations[i].channels.resize(glTFAnimation.channels.size());
    for (size_t j = 0; j < glTFAnimation.channels.size(); j++) {
      tinygltf::AnimationChannel glTFChannel = glTFAnimation.channels[j];
      AnimationChannelGLTF& dstChannel = _animations[i].channels[j];
      dstChannel.path = glTFChannel.target_path;
      dstChannel.samplerIndex = glTFChannel.sampler;
      dstChannel.node = _nodeFromIndex(glTFChannel.target_node);
    }
  }
}

// Helper functions for locating glTF nodes
ModelGLTF::NodeGLTF* ModelGLTF::_findNode(NodeGLTF* parent, uint32_t index) {
  NodeGLTF* nodeFound = nullptr;
  if (parent->index == index) {
    return parent;
  }
  for (auto& child : parent->children) {
    nodeFound = _findNode(child, index);
    if (nodeFound) {
      break;
    }
  }
  return nodeFound;
}

ModelGLTF::NodeGLTF* ModelGLTF::_nodeFromIndex(uint32_t index) {
  NodeGLTF* nodeFound = nullptr;
  for (auto& node : _nodes) {
    nodeFound = _findNode(node, index);
    if (nodeFound) {
      break;
    }
  }
  return nodeFound;
}

void ModelGLTF::_loadNode(tinygltf::Node& input,
                          tinygltf::Model& model,
                          NodeGLTF* parent,
                          uint32_t nodeIndex,
                          std::vector<uint32_t>& indexBuffer,
                          std::vector<Vertex3D>& vertexBuffer) {
  NodeGLTF* node = new NodeGLTF();
  node->parent = parent;
  node->matrix = glm::mat4(1.f);
  node->index = nodeIndex;
  node->skin = input.skin;

  // Get the local node matrix
  // It's either made up from translation, rotation, scale or a 4x4 matrix
  if (input.translation.size() == 3) {
    node->translation = glm::make_vec3(input.translation.data());
  }
  if (input.rotation.size() == 4) {
    glm::quat q = glm::make_quat(input.rotation.data());
    node->rotation = glm::mat4(q);
  }
  if (input.scale.size() == 3) {
    node->scale = glm::make_vec3(input.scale.data());
  }

  if (input.matrix.size() == 16) {
    node->matrix = glm::make_mat4x4(input.matrix.data());
  };

  // Load node's children
  if (input.children.size() > 0) {
    for (size_t i = 0; i < input.children.size(); i++) {
      _loadNode(model.nodes[input.children[i]], model, node, input.children[i], indexBuffer, vertexBuffer);
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
      bool hasSkin = false;
      // Vertices
      {
        const float* positionBuffer = nullptr;
        const float* normalsBuffer = nullptr;
        const float* texCoordsBuffer = nullptr;
        const void* jointIndicesBuffer = nullptr;
        const float* jointWeightsBuffer = nullptr;
        size_t vertexCount = 0;

        int jointByteStride;
        int jointComponentType;
        int positionByteStride;
        int normalByteStride;
        int uv0ByteStride;
        int weightByteStride;
        // Get buffer data for vertex positions
        if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
          const tinygltf::Accessor& accessor = model.accessors[glTFPrimitive.attributes.find("POSITION")->second];
          const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
          positionBuffer = reinterpret_cast<const float*>(
              &(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
          vertexCount = accessor.count;
          positionByteStride = accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(float))
                                                         : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
        }
        // Get buffer data for vertex normals
        if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
          const tinygltf::Accessor& accessor = model.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
          const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
          normalsBuffer = reinterpret_cast<const float*>(
              &(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
          normalByteStride = accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(float))
                                                       : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
        }
        // Get buffer data for vertex texture coordinates
        // glTF supports multiple sets, we only load the first one
        if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
          const tinygltf::Accessor& accessor = model.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
          const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
          texCoordsBuffer = reinterpret_cast<const float*>(
              &(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
          uv0ByteStride = accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(float))
                                                    : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2);
        }

        // POI: Get buffer data required for vertex skinning
        // Get vertex joint indices
        if (glTFPrimitive.attributes.find("JOINTS_0") != glTFPrimitive.attributes.end()) {
          const tinygltf::Accessor& accessor = model.accessors[glTFPrimitive.attributes.find("JOINTS_0")->second];
          const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
          jointIndicesBuffer = &(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);
          jointComponentType = accessor.componentType;
          jointByteStride = accessor.ByteStride(view)
                                ? (accessor.ByteStride(view) / tinygltf::GetComponentSizeInBytes(jointComponentType))
                                : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
        }
        // Get vertex joint weights
        if (glTFPrimitive.attributes.find("WEIGHTS_0") != glTFPrimitive.attributes.end()) {
          const tinygltf::Accessor& accessor = model.accessors[glTFPrimitive.attributes.find("WEIGHTS_0")->second];
          const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
          jointWeightsBuffer = reinterpret_cast<const float*>(
              &(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
          weightByteStride = accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(float))
                                                       : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
        }

        hasSkin = (jointIndicesBuffer && jointWeightsBuffer);

        // Append data to model's vertex buffer
        for (size_t v = 0; v < vertexCount; v++) {
          Vertex3D vert{};
          vert.pos = glm::vec4(glm::make_vec3(&positionBuffer[v * positionByteStride]), 1.0f);
          vert.normal = glm::normalize(
              glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * normalByteStride]) : glm::vec3(0.0f)));
          vert.texCoord = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * uv0ByteStride]) : glm::vec3(0.0f);
          vert.color = glm::vec3(1.0f);
          if (hasSkin) {
            switch (jointComponentType) {
              case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                const uint16_t* buf = static_cast<const uint16_t*>(jointIndicesBuffer);
                vert.jointIndices = glm::vec4(glm::make_vec4(&buf[v * jointByteStride]));
                break;
              }
              case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                const uint8_t* buf = static_cast<const uint8_t*>(jointIndicesBuffer);
                vert.jointIndices = glm::vec4(glm::make_vec4(&buf[v * jointByteStride]));
                break;
              }
              default:
                // Not supported by spec
                std::cerr << "Joint component type " << jointComponentType << " not supported!" << std::endl;
                break;
            }
          } else {
            vert.jointIndices = glm::vec4(0.0f);
          }

          vert.jointWeights = hasSkin ? glm::make_vec4(&jointWeightsBuffer[v * weightByteStride]) : glm::vec4(0.0f);

          if (_materials.size() > 0)
            vert.color = _materials[glTFPrimitive.material].baseColorFactor;
          else
            vert.color = glm::vec3(1.f, 1.f, 1.f);

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

// Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
glm::mat4 ModelGLTF::_getNodeMatrix(NodeGLTF* node) {
  glm::mat4 nodeMatrix = node->getLocalMatrix();
  NodeGLTF* currentParent = node->parent;
  while (currentParent) {
    nodeMatrix = currentParent->getLocalMatrix() * nodeMatrix;
    currentParent = currentParent->parent;
  }
  return nodeMatrix;
}

void ModelGLTF::_updateJoints(NodeGLTF* node) {
  if (node->skin > -1) {
    // Update the joint matrices
    glm::mat4 inverseTransform = glm::inverse(_getNodeMatrix(node));
    SkinGLTF skin = _skins[node->skin];
    _jointsNum = (uint32_t)skin.joints.size();
    std::vector<glm::mat4> jointMatrices(_jointsNum);
    for (size_t i = 0; i < _jointsNum; i++) {
      jointMatrices[i] = _getNodeMatrix(skin.joints[i]) * skin.inverseBindMatrices[i];
      jointMatrices[i] = inverseTransform * jointMatrices[i];
    }

    memcpy(skin.ssbo->getMappedMemory(), jointMatrices.data(), jointMatrices.size() * sizeof(glm::mat4));
  }

  for (auto& child : node->children) {
    _updateJoints(child);
  }
}

// Update the current animation
void ModelGLTF::_updateAnimation(float deltaTime) {
  if (_activeAnimation > static_cast<uint32_t>(_animations.size()) - 1) {
    std::cout << "No animation with index " << _activeAnimation << std::endl;
    return;
  }
  AnimationGLTF& animation = _animations[_activeAnimation];
  animation.currentTime += deltaTime;
  if (animation.currentTime > animation.end) {
    animation.currentTime -= animation.end;
  }

  for (auto& channel : animation.channels) {
    AnimationSamplerGLTF& sampler = animation.samplers[channel.samplerIndex];
    for (size_t i = 0; i < sampler.inputs.size() - 1; i++) {
      if (sampler.interpolation != "LINEAR") {
        std::cout << "This sample only supports linear interpolations\n";
        continue;
      }

      // Get the input keyframe values for the current time stamp
      if ((animation.currentTime >= sampler.inputs[i]) && (animation.currentTime <= sampler.inputs[i + 1])) {
        float a = (animation.currentTime - sampler.inputs[i]) / (sampler.inputs[i + 1] - sampler.inputs[i]);
        if (channel.path == "translation") {
          channel.node->translation = glm::mix(sampler.outputsVec4[i], sampler.outputsVec4[i + 1], a);
        }
        if (channel.path == "rotation") {
          glm::quat q1;
          q1.x = sampler.outputsVec4[i].x;
          q1.y = sampler.outputsVec4[i].y;
          q1.z = sampler.outputsVec4[i].z;
          q1.w = sampler.outputsVec4[i].w;

          glm::quat q2;
          q2.x = sampler.outputsVec4[i + 1].x;
          q2.y = sampler.outputsVec4[i + 1].y;
          q2.z = sampler.outputsVec4[i + 1].z;
          q2.w = sampler.outputsVec4[i + 1].w;

          channel.node->rotation = glm::normalize(glm::slerp(q1, q2, a));
        }
        if (channel.path == "scale") {
          channel.node->scale = glm::mix(sampler.outputsVec4[i], sampler.outputsVec4[i + 1], a);
        }
      }
    }
  }
  for (auto& node : _nodes) {
    _updateJoints(node);
  }
}

void ModelGLTF::setModel(glm::mat4 model) { _model = model; }

void ModelGLTF::setView(glm::mat4 view) { _view = view; }

void ModelGLTF::setProjection(glm::mat4 projection) { _projection = projection; }

void ModelGLTF::_drawNode(int currentFrame, NodeGLTF* node) {
  if (node->mesh.primitives.size() > 0) {
    glm::mat4 nodeMatrix = node->getLocalMatrix();
    NodeGLTF* currentParent = node->parent;
    while (currentParent) {
      nodeMatrix = currentParent->getLocalMatrix() * nodeMatrix;
      currentParent = currentParent->parent;
    }
    // pass this matrix to uniforms
    UniformObject ubo{};
    ubo.model = _model * nodeMatrix;
    ubo.view = _view;
    ubo.projection = _projection;

    void* data;
    vkMapMemory(_device->getLogicalDevice(), _uniformBuffer->getBuffer()[currentFrame]->getMemory(), 0, sizeof(ubo), 0,
                &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(_device->getLogicalDevice(), _uniformBuffer->getBuffer()[currentFrame]->getMemory());

    vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline->getPipelineLayout(), 0, 1,
                            &_descriptorSetCamera->getDescriptorSets()[currentFrame], 0, nullptr);

    if (node->skin >= 0) {
      vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              _pipeline->getPipelineLayout(), 2, 1,
                              &_skins[node->skin].descriptorSet->getDescriptorSets()[currentFrame], 0, nullptr);
    } else {
      vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              _pipeline->getPipelineLayout(), 2, 1,
                              &_descriptorSetJointsDefault->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    for (PrimitiveGLTF& primitive : node->mesh.primitives) {
      if (primitive.indexCount > 0) {
        // Get the texture index for this primitive
        if (_materials.size() > 0 && primitive.materialIndex >= 0) {
          auto material = _materials[primitive.materialIndex];
          if (_textures.size() > 0 && material.baseColorTextureIndex >= 0) {
            TextureGLTF texture = _textures[material.baseColorTextureIndex];
            // Bind the descriptor for the current primitive's texture
            vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    _pipeline->getPipelineLayout(), 1, 1,
                                    &_images[texture.imageIndex].descriptorSet->getDescriptorSets()[currentFrame], 0,
                                    nullptr);
          }
        }
        vkCmdDrawIndexed(_commandBuffer->getCommandBuffer()[currentFrame], primitive.indexCount, 1,
                         primitive.firstIndex, 0, 0);
      }
    }
  }

  for (auto& child : node->children) {
    _drawNode(currentFrame, child);
  }
}

void ModelGLTF::draw(float frameTimer, int currentFrame) {
  PushConstants pushConstants;
  pushConstants.jointNum = _jointsNum;
  vkCmdPushConstants(_commandBuffer->getCommandBuffer()[currentFrame], _pipeline->getPipelineLayout(),
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);

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

  if (_animations.size() > 0) _updateAnimation(frameTimer);
}