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

void Model::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }
void Model::setModel(glm::mat4 model) { _model = model; }

struct ModelAuxilary {
  VkBool32 alphaMask;
  float alphaMaskCutoff;
};

ModelGLTF::ModelGLTF(std::string path,
                     std::shared_ptr<Texture> shadowMap,
                     std::vector<std::shared_ptr<DescriptorSetLayout>> descriptorSetLayout,
                     std::shared_ptr<LightManager> lightManager,
                     std::shared_ptr<RenderPass> renderPass,
                     std::shared_ptr<DescriptorPool> descriptorPool,
                     std::shared_ptr<CommandPool> commandPool,
                     std::shared_ptr<CommandBuffer> commandBuffer,
                     std::shared_ptr<Queue> queue,
                     std::shared_ptr<Device> device,
                     std::shared_ptr<Settings> settings) {
  _device = device;
  _queue = queue;
  _commandPool = commandPool;
  _descriptorPool = descriptorPool;
  _commandBuffer = commandBuffer;
  _lightManager = lightManager;

  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;
  std::vector<uint32_t> indexBuffer;
  std::vector<Vertex3D> vertexBuffer;
  bool loaded = loader.LoadASCIIFromFile(&model, &err, &warn, path);
  if (loaded == false) throw std::runtime_error("Can't load model");
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

  //********************************************************************
  _vertexBuffer = std::make_shared<VertexBuffer3D>(vertexBuffer, commandPool, queue, device);
  _indexBuffer = std::make_shared<IndexBuffer>(indexBuffer, commandPool, queue, device);
  _uniformBuffer[ModelRenderMode::DEPTH] = std::make_shared<UniformBuffer>(
      settings->getMaxFramesInFlight(), sizeof(UniformObject), commandPool, queue, device);
  _uniformBuffer[ModelRenderMode::FULL] = std::make_shared<UniformBuffer>(
      settings->getMaxFramesInFlight(), sizeof(UniformObject), commandPool, queue, device);

  _descriptorSetCamera[ModelRenderMode::DEPTH] = std::make_shared<DescriptorSet>(
      settings->getMaxFramesInFlight(), descriptorSetLayout[0], descriptorPool, device);
  _descriptorSetCamera[ModelRenderMode::DEPTH]->createCamera(_uniformBuffer[ModelRenderMode::DEPTH]);

  _descriptorSetCamera[ModelRenderMode::FULL] = std::make_shared<DescriptorSet>(
      settings->getMaxFramesInFlight(), descriptorSetLayout[0], descriptorPool, device);
  _descriptorSetCamera[ModelRenderMode::FULL]->createCamera(_uniformBuffer[ModelRenderMode::FULL]);

  _stubTexture = std::make_shared<Texture>("../data/Texture1x1.png", commandPool, queue, device);
  for (auto& material : _materials) {
    material.descriptorSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), descriptorSetLayout[1],
                                                             _descriptorPool, _device);
    auto diffuseTexture = _stubTexture;
    auto normalTexture = _stubTexture;
    auto shadowTexture = _stubTexture;
    if (material.baseColorTextureIndex < _textures.size()) {
      diffuseTexture = _images[_textures[material.baseColorTextureIndex].imageIndex].texture;
      normalTexture = _images[_textures[material.normalTextureIndex].imageIndex].texture;
    }
    if (shadowMap) shadowTexture = shadowMap;
    material.descriptorSet->createGraphicModel(diffuseTexture, normalTexture, shadowTexture);

    material.bufferModelAuxilary = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(),
                                                                   sizeof(ModelAuxilary), commandPool, queue, device);
    material.descriptorSetModelAuxilary = std::make_shared<DescriptorSet>(
        settings->getMaxFramesInFlight(), descriptorSetLayout[3], descriptorPool, device);
    material.descriptorSetModelAuxilary->createModelAuxilary(material.bufferModelAuxilary);
  }

  _descriptorSetJointsDefault = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(),
                                                                descriptorSetLayout[2], _descriptorPool, _device);
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
    skin.descriptorSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), descriptorSetLayout[2],
                                                         _descriptorPool, _device);
    skin.descriptorSet->createJoints(skin.ssbo);
  }
}

void ModelGLTF::_loadImages(tinygltf::Model& model) {
  _images.resize(model.images.size());
  for (int i = 0; i < model.images.size(); i++) {
    tinygltf::Image& glTFImage = model.images[i];
    // TODO: check if such file exists and load it from disk
    if (std::filesystem::exists(glTFImage.uri)) {
      _images[i].texture = std::make_shared<Texture>(glTFImage.uri, _commandPool, _queue, _device);
    } else {
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
    // Get normal texture
    if (glTFMaterial.additionalValues.find("normalTexture") != glTFMaterial.additionalValues.end()) {
      _materials[i].normalTextureIndex = glTFMaterial.additionalValues["normalTexture"].TextureIndex();
    }

    _materials[i].doubleSided = glTFMaterial.doubleSided;
    _materials[i].alphaMask = (glTFMaterial.alphaMode == "MASK");
    _materials[i].alphaMaskCutoff = glTFMaterial.alphaCutoff;
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
        const float* tangentsBuffer = nullptr;
        size_t vertexCount = 0;

        int jointByteStride;
        int jointComponentType;
        int positionByteStride;
        int normalByteStride;
        int uv0ByteStride;
        int weightByteStride;
        int tangentByteStride;
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

        if (glTFPrimitive.attributes.find("TANGENT") != glTFPrimitive.attributes.end()) {
          const tinygltf::Accessor& accessor = model.accessors[glTFPrimitive.attributes.find("TANGENT")->second];
          const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
          tangentsBuffer = reinterpret_cast<const float*>(
              &(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
          tangentByteStride = accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(float))
                                                        : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
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

          vert.tangent = tangentsBuffer ? glm::make_vec4(&tangentsBuffer[v * tangentByteStride]) : glm::vec4(0.0f);

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

void ModelGLTF::_drawNode(int currentFrame,
                          ModelRenderMode mode,
                          std::shared_ptr<Pipeline> pipeline,
                          std::shared_ptr<Pipeline> pipelineCullOff,
                          NodeGLTF* node) {
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
    ubo.view = _camera->getView();
    ubo.projection = _camera->getProjection();

    void* data;
    vkMapMemory(_device->getLogicalDevice(), _uniformBuffer[mode]->getBuffer()[currentFrame]->getMemory(), 0,
                sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(_device->getLogicalDevice(), _uniformBuffer[mode]->getBuffer()[currentFrame]->getMemory());

    if (pipeline->getDescriptorSetLayout().size() > 0) {
      vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 0, 1,
                              &_descriptorSetCamera[mode]->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    if (node->skin >= 0) {
      if (pipeline->getDescriptorSetLayout().size() > 2) {
        vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->getPipelineLayout(), 2, 1,
                                &_skins[node->skin].descriptorSet->getDescriptorSets()[currentFrame], 0, nullptr);
      }
    } else {
      if (pipeline->getDescriptorSetLayout().size() > 2) {
        vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->getPipelineLayout(), 2, 1,
                                &_descriptorSetJointsDefault->getDescriptorSets()[currentFrame], 0, nullptr);
      }
    }

    for (PrimitiveGLTF& primitive : node->mesh.primitives) {
      if (primitive.indexCount > 0) {
        // Get the texture index for this primitive
        if (_materials.size() > 0 && primitive.materialIndex >= 0) {
          auto material = _materials[primitive.materialIndex];

          // pass this matrix to uniforms
          ModelAuxilary aux{};
          aux.alphaMask = material.alphaMask;
          aux.alphaMaskCutoff = material.alphaMaskCutoff;

          void* data;
          vkMapMemory(_device->getLogicalDevice(), material.bufferModelAuxilary->getBuffer()[currentFrame]->getMemory(),
                      0, sizeof(aux), 0, &data);
          memcpy(data, &aux, sizeof(aux));
          vkUnmapMemory(_device->getLogicalDevice(),
                        material.bufferModelAuxilary->getBuffer()[currentFrame]->getMemory());

          if (pipeline->getDescriptorSetLayout().size() > 3) {
            vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline->getPipelineLayout(), 3, 1,
                                    &material.descriptorSetModelAuxilary->getDescriptorSets()[currentFrame], 0,
                                    nullptr);
          }

          {
            auto currentPipeline = pipeline;
            if (material.doubleSided) currentPipeline = pipelineCullOff;
            vkCmdBindPipeline(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              currentPipeline->getPipeline());
            if (pipeline->getDescriptorSetLayout().size() > 1) {
              vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      currentPipeline->getPipelineLayout(), 1, 1,
                                      &material.descriptorSet->getDescriptorSets()[currentFrame], 0, nullptr);
            }
          }
        } else {
          vkCmdBindPipeline(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->getPipeline());
        }
        vkCmdDrawIndexed(_commandBuffer->getCommandBuffer()[currentFrame], primitive.indexCount, 1,
                         primitive.firstIndex, 0, 0);
      }
    }
  }

  for (auto& child : node->children) {
    _drawNode(currentFrame, mode, pipeline, pipelineCullOff, child);
  }
}

void ModelGLTF::draw(int currentFrame,
                     ModelRenderMode mode,
                     std::shared_ptr<Pipeline> pipeline,
                     std::shared_ptr<Pipeline> pipelineCullOff,
                     float frameTimer) {
  if (pipeline->getPushConstants().find("vertex") != pipeline->getPushConstants().end()) {
    PushConstants pushConstants;
    pushConstants.jointNum = _jointsNum;
    vkCmdPushConstants(_commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT, sizeof(LightPush), sizeof(PushConstants), &pushConstants);
  }

  // All vertices and indices are stored in single buffers, so we only need to bind once
  VkBuffer vertexBuffers[] = {_vertexBuffer->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(_commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);
  vkCmdBindIndexBuffer(_commandBuffer->getCommandBuffer()[currentFrame], _indexBuffer->getBuffer()->getData(), 0,
                       VK_INDEX_TYPE_UINT32);
  // Render all nodes at top-level
  for (auto& node : _nodes) {
    _drawNode(currentFrame, mode, pipeline, pipelineCullOff, node);
  }

  if (_animations.size() > 0) _updateAnimation(frameTimer);
}