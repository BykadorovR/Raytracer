#define TINYOBJLOADER_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "Loader.h"
#include "glm/gtc/type_ptr.hpp"
#include <filesystem>

Loader::Loader(std::string path, std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state) {
  _path = path;
  _commandBufferTransfer = commandBufferTransfer;
  _state = state;

  std::string err, warn;
  bool loaded = _loader.LoadASCIIFromFile(&_model, &err, &warn, path);
  if (loaded == false) throw std::runtime_error("Can't load model");

  // allocate meshes
  for (int i = 0; i < _model.meshes.size(); i++) {
    auto mesh = std::make_shared<Mesh3D>(_state);
    _meshes.push_back(mesh);
  }

  // load material
  _loadTextures();
  _loadMaterials();
  // load nodes
  const tinygltf::Scene& scene = _model.scenes[0];
  for (size_t i = 0; i < scene.nodes.size(); i++) {
    tinygltf::Node& node = _model.nodes[scene.nodes[i]];
    _loadNode(node, nullptr, scene.nodes[i]);
  }
  // load bones/joints
  _loadSkins();
  // load animations
  _loadAnimations();
}

// load all textures here
void Loader::_loadTextures() {
  for (int i = 0; i < _model.images.size(); i++) {
    tinygltf::Image& glTFImage = _model.images[i];
    std::shared_ptr<Texture> texture;
    if (std::filesystem::exists(glTFImage.uri)) {
      texture = std::make_shared<Texture>(glTFImage.uri, _state->getSettings()->getLoadTextureColorFormat(),
                                          VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, _commandBufferTransfer,
                                          _state->getSettings(), _state->getDevice());
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
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());

      void* data;
      vkMapMemory(_state->getDevice()->getLogicalDevice(), stagingBuffer->getMemory(), 0, bufferSize, 0, &data);
      memcpy(data, buffer, bufferSize);
      vkUnmapMemory(_state->getDevice()->getLogicalDevice(), stagingBuffer->getMemory());

      auto image = std::make_shared<Image>(std::tuple{glTFImage.width, glTFImage.height}, 1, 1,
                                           _state->getSettings()->getLoadTextureColorFormat(), VK_IMAGE_TILING_OPTIMAL,
                                           VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _state->getDevice());

      image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 1,
                          1, _commandBufferTransfer);
      image->copyFrom(stagingBuffer, 1, _commandBufferTransfer);
      image->changeLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1,
                          1, _commandBufferTransfer);

      auto imageView = std::make_shared<ImageView>(image, VK_IMAGE_VIEW_TYPE_2D, 1, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT,
                                                   _state->getDevice());
      texture = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_REPEAT, imageView, _state->getDevice());

      if (deleteBuffer) {
        delete[] buffer;
      }
    }
    _textures.push_back(texture);
  }
}

// TODO: we can store baseColorFactor only in GLTF material, rest will go to PBR/Phong
void Loader::_loadMaterials() {
  for (size_t i = 0; i < _model.materials.size(); i++) {
    tinygltf::Material glTFMaterial = _model.materials[i];
    std::shared_ptr<MaterialPhong> materialPhong = std::make_shared<MaterialPhong>(_commandBufferTransfer, _state);
    std::shared_ptr<MaterialPBR> materialPBR = std::make_shared<MaterialPBR>(_commandBufferTransfer, _state);
    std::shared_ptr<MaterialGLTF> material = std::make_shared<MaterialGLTF>();
    float metallicFactor = 0;
    float roughnessFactor = 0;
    float occlusionStrength = 0;
    glm::vec3 emissiveFactor = glm::vec3(0.f);
    // Get the base color factor
    material->baseColorFactor = glm::make_vec4(glTFMaterial.pbrMetallicRoughness.baseColorFactor.data());
    // Get metallic factor
    metallicFactor = glTFMaterial.pbrMetallicRoughness.metallicFactor;
    // Get roughness factor
    roughnessFactor = glTFMaterial.pbrMetallicRoughness.roughnessFactor;
    // Get occlusion strength
    occlusionStrength = glTFMaterial.occlusionTexture.strength;
    // Get emissive factor
    emissiveFactor = glm::make_vec3(glTFMaterial.emissiveFactor.data());
    // Get double side propery
    materialPBR->setDoubleSided(glTFMaterial.doubleSided);
    materialPhong->setDoubleSided(glTFMaterial.doubleSided);
    // Get alpha cut off information
    materialPBR->setAlphaCutoff((glTFMaterial.alphaMode == "MASK"), glTFMaterial.alphaCutoff);
    materialPhong->setAlphaCutoff((glTFMaterial.alphaMode == "MASK"), glTFMaterial.alphaCutoff);
    // Get base color texture
    {
      // glTF texture index
      auto baseColorTextureIndex = glTFMaterial.pbrMetallicRoughness.baseColorTexture.index;
      if (baseColorTextureIndex >= 0) {
        // glTF image index
        auto baseColorImageIndex = _model.textures[baseColorTextureIndex].source;
        // set texture to phong material
        materialPhong->setBaseColor(_textures[baseColorImageIndex]);
        // set texture to PBR material
        materialPBR->setBaseColor(_textures[baseColorImageIndex]);
      }
    }
    // Get normal texture
    {
      auto normalTextureIndex = glTFMaterial.normalTexture.index;
      if (normalTextureIndex >= 0) {
        // glTF image index
        auto normalImageIndex = _model.textures[normalTextureIndex].source;
        // set normal texture to phong material
        materialPhong->setNormal(_textures[normalImageIndex]);
        // set normal texture to PBR material
        materialPBR->setNormal(_textures[normalImageIndex]);
      }
    }
    // Get metallic-roughness texture
    {
      auto metallicRoughnessTextureIndex = glTFMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
      if (metallicRoughnessTextureIndex >= 0) {
        // glTF image index
        auto metallicRoughnessImageIndex = _model.textures[metallicRoughnessTextureIndex].source;
        // set PBR texture to PBR material
        materialPBR->setMetallicRoughness(_textures[metallicRoughnessImageIndex]);
      }
    }
    // Get occlusion texture
    {
      auto occlusionTextureIndex = glTFMaterial.occlusionTexture.index;
      if (occlusionTextureIndex >= 0) {
        auto occlusionImageIndex = _model.textures[occlusionTextureIndex].source;
        materialPBR->setOccluded(_textures[occlusionImageIndex]);
      }
    }
    // Get emissive texture
    {
      auto emissiveTextureIndex = glTFMaterial.emissiveTexture.index;
      if (emissiveTextureIndex >= 0) {
        auto emissiveImageIndex = _model.textures[emissiveTextureIndex].source;
        materialPBR->setEmissive(_textures[emissiveImageIndex]);
      }
    }

    _materials.push_back(material);
    _materialsPhong.push_back(materialPhong);
    _materialsPBR.push_back(materialPBR);
  }
}

void Loader::_loadNode(tinygltf::Node& input, std::shared_ptr<NodeGLTF> parent, uint32_t nodeIndex) {
  std::shared_ptr<NodeGLTF> node = std::make_shared<NodeGLTF>();
  node->parent = parent;
  node->matrix = glm::mat4(1.f);
  node->index = nodeIndex;
  node->skin = input.skin;
  node->mesh = input.mesh;

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
      _loadNode(_model.nodes[input.children[i]], node, input.children[i]);
    }
  }

  // If the node contains mesh data, we load vertices and indices from the buffers
  // In glTF this is done via accessors and buffer views
  if (input.mesh > -1) {
    // GLTF mesh
    const tinygltf::Mesh meshGLTF = _model.meshes[input.mesh];
    // internal engine mesh
    auto mesh = _meshes[input.mesh];
    std::vector<uint32_t> indexes;
    std::vector<Vertex3D> vertices;
    // Iterate through all primitives of this node's mesh
    for (size_t i = 0; i < meshGLTF.primitives.size(); i++) {
      const tinygltf::Primitive& glTFPrimitive = meshGLTF.primitives[i];
      uint32_t firstIndex = indexes.size();
      uint32_t vertexStart = vertices.size();
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
          const tinygltf::Accessor& accessor = _model.accessors[glTFPrimitive.attributes.find("POSITION")->second];
          const tinygltf::BufferView& view = _model.bufferViews[accessor.bufferView];
          positionBuffer = reinterpret_cast<const float*>(
              &(_model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
          vertexCount = accessor.count;
          positionByteStride = accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(float))
                                                         : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
        }
        // Get buffer data for vertex normals
        if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
          const tinygltf::Accessor& accessor = _model.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
          const tinygltf::BufferView& view = _model.bufferViews[accessor.bufferView];
          normalsBuffer = reinterpret_cast<const float*>(
              &(_model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
          normalByteStride = accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(float))
                                                       : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
        }
        // Get buffer data for vertex texture coordinates
        // glTF supports multiple sets, we only load the first one
        if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
          const tinygltf::Accessor& accessor = _model.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
          const tinygltf::BufferView& view = _model.bufferViews[accessor.bufferView];
          texCoordsBuffer = reinterpret_cast<const float*>(
              &(_model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
          uv0ByteStride = accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(float))
                                                    : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2);
        }

        if (glTFPrimitive.attributes.find("TANGENT") != glTFPrimitive.attributes.end()) {
          const tinygltf::Accessor& accessor = _model.accessors[glTFPrimitive.attributes.find("TANGENT")->second];
          const tinygltf::BufferView& view = _model.bufferViews[accessor.bufferView];
          tangentsBuffer = reinterpret_cast<const float*>(
              &(_model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
          tangentByteStride = accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(float))
                                                        : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
        }

        // Get vertex joint indices
        if (glTFPrimitive.attributes.find("JOINTS_0") != glTFPrimitive.attributes.end()) {
          const tinygltf::Accessor& accessor = _model.accessors[glTFPrimitive.attributes.find("JOINTS_0")->second];
          const tinygltf::BufferView& view = _model.bufferViews[accessor.bufferView];
          jointIndicesBuffer = &(_model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);
          jointComponentType = accessor.componentType;
          jointByteStride = accessor.ByteStride(view)
                                ? (accessor.ByteStride(view) / tinygltf::GetComponentSizeInBytes(jointComponentType))
                                : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
        }
        // Get vertex joint weights
        if (glTFPrimitive.attributes.find("WEIGHTS_0") != glTFPrimitive.attributes.end()) {
          const tinygltf::Accessor& accessor = _model.accessors[glTFPrimitive.attributes.find("WEIGHTS_0")->second];
          const tinygltf::BufferView& view = _model.bufferViews[accessor.bufferView];
          jointWeightsBuffer = reinterpret_cast<const float*>(
              &(_model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
          weightByteStride = accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(float))
                                                       : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
        }

        hasSkin = (jointIndicesBuffer && jointWeightsBuffer);

        // Append data to model's vertex buffer
        for (size_t v = 0; v < vertexCount; v++) {
          Vertex3D vertex{};
          vertex.pos = glm::vec4(glm::make_vec3(&positionBuffer[v * positionByteStride]), 1.0f);
          // default value
          vertex.normal = glm::vec3(0.f, 0.f, 1.f);
          if (normalsBuffer) {
            vertex.normal = glm::normalize(glm::vec3(glm::make_vec3(&normalsBuffer[v * normalByteStride])));
          }
          vertex.texCoord = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * uv0ByteStride]) : glm::vec3(0.0f);
          vertex.jointIndices = glm::vec4(0.0f);
          vertex.jointWeights = glm::vec4(0.0f);
          if (hasSkin) {
            vertex.jointWeights = glm::make_vec4(&jointWeightsBuffer[v * weightByteStride]);
            switch (jointComponentType) {
              case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                const uint16_t* buf = static_cast<const uint16_t*>(jointIndicesBuffer);
                vertex.jointIndices = glm::vec4(glm::make_vec4(&buf[v * jointByteStride]));
                break;
              }
              case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                const uint8_t* buf = static_cast<const uint8_t*>(jointIndicesBuffer);
                vertex.jointIndices = glm::vec4(glm::make_vec4(&buf[v * jointByteStride]));
                break;
              }
              default:
                // Not supported by spec
                std::cerr << "Joint component type " << jointComponentType << " not supported!" << std::endl;
                break;
            }
          }
          vertex.color = glm::vec3(1.f);
          if (_materials.size() > glTFPrimitive.material)
            vertex.color = _materials[glTFPrimitive.material]->baseColorFactor;

          vertex.tangent = glm::vec4(0.0f);
          if (tangentsBuffer) vertex.tangent = glm::make_vec4(&tangentsBuffer[v * tangentByteStride]);

          vertices.push_back(vertex);
        }
      }
      // Indices
      {
        const tinygltf::Accessor& accessor = _model.accessors[glTFPrimitive.indices];
        const tinygltf::BufferView& bufferView = _model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = _model.buffers[bufferView.buffer];

        indexCount += static_cast<uint32_t>(accessor.count);

        // glTF supports different component types of indices
        // TODO: check + vertexStart, what is it
        switch (accessor.componentType) {
          case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
            const uint32_t* buf = reinterpret_cast<const uint32_t*>(
                &buffer.data[accessor.byteOffset + bufferView.byteOffset]);
            for (size_t index = 0; index < accessor.count; index++) {
              indexes.push_back(buf[index] + vertexStart);
            }
            break;
          }
          case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
            const uint16_t* buf = reinterpret_cast<const uint16_t*>(
                &buffer.data[accessor.byteOffset + bufferView.byteOffset]);
            for (size_t index = 0; index < accessor.count; index++) {
              indexes.push_back(buf[index] + vertexStart);
            }
            break;
          }
          case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
            const uint8_t* buf = reinterpret_cast<const uint8_t*>(
                &buffer.data[accessor.byteOffset + bufferView.byteOffset]);
            for (size_t index = 0; index < accessor.count; index++) {
              indexes.push_back(buf[index] + vertexStart);
            }
            break;
          }
          default:
            std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
            return;
        }
      }
      MeshPrimitive primitive{};
      primitive.firstIndex = firstIndex;
      primitive.indexCount = indexCount;
      primitive.materialIndex = glTFPrimitive.material;
      mesh->addPrimitive(primitive);
    }

    mesh->setIndexes(indexes, _commandBufferTransfer);
    mesh->setVertices(vertices, _commandBufferTransfer);
  }

  // we store all node's heads in _nodes array
  // if parent exists just attach node to existing node tree
  if (parent) {
    parent->children.push_back(node);
  } else {
    _nodes.push_back(node);
  }
}

// Helper functions for locating glTF nodes
std::shared_ptr<NodeGLTF> Loader::_findNode(std::shared_ptr<NodeGLTF> parent, uint32_t index) {
  std::shared_ptr<NodeGLTF> nodeFound = nullptr;
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

std::shared_ptr<NodeGLTF> Loader::_nodeFromIndex(uint32_t index) {
  std::shared_ptr<NodeGLTF> nodeFound = nullptr;
  for (auto& node : _nodes) {
    nodeFound = _findNode(node, index);
    if (nodeFound) {
      break;
    }
  }
  return nodeFound;
}

// TODO: ssbo matrix copy is missing
void Loader::_loadSkins() {
  for (size_t i = 0; i < _model.skins.size(); i++) {
    std::shared_ptr<SkinGLTF> skin = std::make_shared<SkinGLTF>();
    tinygltf::Skin glTFSkin = _model.skins[i];
    skin->name = glTFSkin.name;
    // Find joint nodes
    for (int jointIndex : glTFSkin.joints) {
      auto node = _nodeFromIndex(jointIndex);
      if (node) {
        skin->joints.push_back(node);
      }
    }

    // Get the inverse bind matrices from the buffer associated to this skin
    if (glTFSkin.inverseBindMatrices > -1) {
      const tinygltf::Accessor& accessor = _model.accessors[glTFSkin.inverseBindMatrices];
      const tinygltf::BufferView& bufferView = _model.bufferViews[accessor.bufferView];
      const tinygltf::Buffer& buffer = _model.buffers[bufferView.buffer];
      skin->inverseBindMatrices.resize(accessor.count);
      std::memcpy(skin->inverseBindMatrices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset],
                  accessor.count * sizeof(glm::mat4));
    }

    _skins.push_back(skin);
  }
}

void Loader::_loadAnimations() {
  for (size_t i = 0; i < _model.animations.size(); i++) {
    std::shared_ptr<AnimationGLTF> animation = std::make_shared<AnimationGLTF>();
    tinygltf::Animation glTFAnimation = _model.animations[i];
    animation->name = glTFAnimation.name;

    // Samplers
    animation->samplers.resize(glTFAnimation.samplers.size());
    for (size_t j = 0; j < glTFAnimation.samplers.size(); j++) {
      tinygltf::AnimationSampler glTFSampler = glTFAnimation.samplers[j];
      AnimationSamplerGLTF& dstSampler = animation->samplers[j];
      dstSampler.interpolation = glTFSampler.interpolation;

      // Read sampler keyframe input time values
      {
        const tinygltf::Accessor& accessor = _model.accessors[glTFSampler.input];
        const tinygltf::BufferView& bufferView = _model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = _model.buffers[bufferView.buffer];
        const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
        const float* buf = static_cast<const float*>(dataPtr);
        for (size_t index = 0; index < accessor.count; index++) {
          dstSampler.inputs.push_back(buf[index]);
        }
        // Adjust animation's start and end times
        for (auto input : animation->samplers[j].inputs) {
          if (input < animation->start) {
            animation->start = input;
          };
          if (input > animation->end) {
            animation->end = input;
          }
        }
      }

      // Read sampler keyframe output translate/rotate/scale values
      {
        const tinygltf::Accessor& accessor = _model.accessors[glTFSampler.output];
        const tinygltf::BufferView& bufferView = _model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = _model.buffers[bufferView.buffer];
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
    animation->channels.resize(glTFAnimation.channels.size());
    for (size_t j = 0; j < glTFAnimation.channels.size(); j++) {
      tinygltf::AnimationChannel glTFChannel = glTFAnimation.channels[j];
      AnimationChannelGLTF& dstChannel = animation->channels[j];
      dstChannel.path = glTFChannel.target_path;
      dstChannel.samplerIndex = glTFChannel.sampler;
      dstChannel.node = _nodeFromIndex(glTFChannel.target_node);
    }

    _animations.push_back(animation);
  }
}

std::vector<std::shared_ptr<MaterialPhong>> Loader::getMaterialsPhong() { return _materialsPhong; }

std::vector<std::shared_ptr<MaterialPBR>> Loader::getMaterialsPBR() { return _materialsPBR; }

const std::vector<std::shared_ptr<SkinGLTF>>& Loader::getSkins() { return _skins; }

const std::vector<std::shared_ptr<AnimationGLTF>>& Loader::getAnimations() { return _animations; }

const std::vector<std::shared_ptr<Mesh3D>>& Loader::getMeshes() { return _meshes; }

const std::vector<std::shared_ptr<NodeGLTF>>& Loader::getNodes() { return _nodes; }