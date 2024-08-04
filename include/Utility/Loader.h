#pragma once
#include "State.h"
#ifdef __ANDROID__
#define TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#endif
#include "tiny_gltf.h"
#include "Material.h"
#include "Mesh.h"
#include <filesystem>

class LoaderImage {
 private:
  std::shared_ptr<State> _state;
  std::map<std::string, std::shared_ptr<BufferImage>> _images;

 public:
  LoaderImage(std::shared_ptr<State> state);
  template <class T>
  std::tuple<std::shared_ptr<T[]>, std::tuple<int, int, int>> loadCPU(std::string path);
  template <class T>
  std::shared_ptr<BufferImage> loadGPU(std::vector<std::string> paths,
                                       std::shared_ptr<CommandBuffer> commandBufferTransfer) {
    for (auto& path : paths) {
      if (_images.contains(path) == false) {
        auto [pixels, dimension] = loadCPU<T>({path});
        VkDeviceSize imageSize = std::get<0>(dimension) * std::get<1>(dimension) * std::get<2>(dimension);
        // fill buffer
        _images[path] = std::make_shared<BufferImage>(
            std::tuple{std::get<0>(dimension), std::get<1>(dimension)}, 4, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);
        _images[path]->setData(pixels.get());
      }
    }

    std::shared_ptr<BufferImage> bufferDst = _images[paths.front()];
    if (paths.size() > 1) {
      bufferDst = std::make_shared<BufferImage>(
          bufferDst->getResolution(), bufferDst->getChannels(), paths.size(),
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);
      for (int i = 0; i < paths.size(); i++) {
        auto bufferSrc = _images[paths[i]];
        bufferDst->copyFrom(bufferSrc, 0, i * bufferSrc->getSize(), commandBufferTransfer);
      }
      // barrier for further image copy from buffer
      VkMemoryBarrier memoryBarrier = {};
      memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
      memoryBarrier.pNext = nullptr;
      memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      vkCmdPipelineBarrier(commandBufferTransfer->getCommandBuffer()[_state->getFrameInFlight()],
                           VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &memoryBarrier, 0,
                           nullptr, 0, nullptr);
    }

    return bufferDst;
  }
};

// A node represents an object in the glTF scene graph
struct NodeGLTF {
  std::shared_ptr<NodeGLTF> parent;
  // node index
  uint32_t index;
  std::vector<std::shared_ptr<NodeGLTF>> children;
  // index in Mesh3D vector
  int mesh = -1;

  glm::vec3 translation{};
  glm::vec3 scale{1.0f};
  glm::quat rotation{};
  // either transforms above or matrix
  glm::mat4 matrix;

  int32_t skin = -1;

  /* Get a node's local matrix from the current translation, rotation and scale values.
  These are calculated from the current animation and need to be calculated dynamically
  We multiply everything because it's either transforms are unit vectores or matrix.
  */
  glm::mat4 getLocalMatrix() {
    return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) *
           matrix;
  }
};

struct MaterialGLTF {
  // PBR info
  glm::vec4 baseColorFactor = glm::vec4(1.0f);  // default [1,1,1,1]
};

struct SkinGLTF {
  std::string name;
  std::vector<glm::mat4> inverseBindMatrices;
  std::vector<std::shared_ptr<NodeGLTF>> joints;
};

struct AnimationSamplerGLTF {
  std::string interpolation;
  std::vector<float> inputs;
  std::vector<glm::vec4> outputsVec4;
};

struct AnimationChannelGLTF {
  std::string path;
  std::shared_ptr<NodeGLTF> node;
  uint32_t samplerIndex;
};

struct AnimationGLTF {
  std::string name;
  std::vector<AnimationSamplerGLTF> samplers;
  std::vector<AnimationChannelGLTF> channels;
  float start = std::numeric_limits<float>::max();
  float end = std::numeric_limits<float>::min();
  float currentTime = 0.0f;
};

class ModelGLTF {
 private:
  std::vector<std::shared_ptr<Texture>> _textures;
  std::vector<std::shared_ptr<NodeGLTF>> _nodes;
  std::vector<std::shared_ptr<MaterialGLTF>> _materials;
  std::vector<std::shared_ptr<MaterialPhong>> _materialsPhong;
  std::vector<std::shared_ptr<MaterialColor>> _materialsColor;
  std::vector<std::shared_ptr<MaterialPBR>> _materialsPBR;
  std::vector<std::shared_ptr<SkinGLTF>> _skins;
  std::vector<std::shared_ptr<AnimationGLTF>> _animations;
  std::vector<std::shared_ptr<Mesh3D>> _meshes;

 public:
  void setMaterialsColor(std::vector<std::shared_ptr<MaterialColor>>& materialsColor);
  void setMaterialsPhong(std::vector<std::shared_ptr<MaterialPhong>>& materialsPhong);
  void setMaterialsPBR(std::vector<std::shared_ptr<MaterialPBR>>& materialsPBR);
  void setSkins(std::vector<std::shared_ptr<SkinGLTF>>& skins);
  void setAnimations(std::vector<std::shared_ptr<AnimationGLTF>>& animations);
  void setNodes(std::vector<std::shared_ptr<NodeGLTF>>& nodes);
  void setMeshes(std::vector<std::shared_ptr<Mesh3D>>& meshes);

  const std::vector<std::shared_ptr<MaterialColor>>& getMaterialsColor();
  const std::vector<std::shared_ptr<MaterialPhong>>& getMaterialsPhong();
  const std::vector<std::shared_ptr<MaterialPBR>>& getMaterialsPBR();
  const std::vector<std::shared_ptr<SkinGLTF>>& getSkins();
  const std::vector<std::shared_ptr<AnimationGLTF>>& getAnimations();
  // one mesh - one node
  const std::vector<std::shared_ptr<NodeGLTF>>& getNodes();
  const std::vector<std::shared_ptr<Mesh3D>>& getMeshes();
};

class LoaderGLTF {
 private:
  std::filesystem::path _path;
  std::shared_ptr<State> _state;
  std::shared_ptr<LoaderImage> _loaderImage;
  tinygltf::TinyGLTF _loader;
  std::map<std::string, std::shared_ptr<ModelGLTF>> _models;

  std::shared_ptr<Texture> _loadTexture(int imageIndex,
                                        VkFormat format,
                                        const tinygltf::Model& modelInternal,
                                        std::vector<std::shared_ptr<Texture>>& textures,
                                        std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void _loadMaterials(const tinygltf::Model& modelInternal,
                      std::vector<std::shared_ptr<MaterialGLTF>>& materialGLTF,
                      std::shared_ptr<ModelGLTF> modelExternal,
                      std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void _loadAnimations(const tinygltf::Model& modelInternal,
                       const std::vector<std::shared_ptr<NodeGLTF>>& nodes,
                       std::vector<std::shared_ptr<AnimationGLTF>>& animations);
  void _loadSkins(const tinygltf::Model& modelInternal,
                  const std::vector<std::shared_ptr<NodeGLTF>>& nodes,
                  std::vector<std::shared_ptr<SkinGLTF>>& skins);
  void _generateTangent(std::vector<uint32_t>& indexes, std::vector<Vertex3D>& vertices);

  std::shared_ptr<NodeGLTF> _findNode(std::shared_ptr<NodeGLTF> parent, uint32_t index);
  std::shared_ptr<NodeGLTF> _nodeFromIndex(uint32_t index, const std::vector<std::shared_ptr<NodeGLTF>>& nodes);
  void _loadNode(const tinygltf::Model& modelInternal,
                 const tinygltf::Node& input,
                 std::shared_ptr<NodeGLTF> parent,
                 uint32_t nodeIndex,
                 const std::vector<std::shared_ptr<MaterialGLTF>>& materials,
                 const std::vector<std::shared_ptr<Mesh3D>>& meshes,
                 std::vector<std::shared_ptr<NodeGLTF>>& nodes,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer);

 public:
  LoaderGLTF(std::shared_ptr<LoaderImage> loaderImage, std::shared_ptr<State> state);
#ifdef __ANDROID__
  void setAssetManager(AAssetManager* assetManager);
#endif
  std::shared_ptr<ModelGLTF> load(std::string path, std::shared_ptr<CommandBuffer> commandBufferTransfer);
};