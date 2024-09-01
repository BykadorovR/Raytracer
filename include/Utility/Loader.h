#pragma once
#include "Utility/State.h"
#include "Graphic/Material.h"
#include "Primitive/Mesh.h"
#ifdef __ANDROID__
#define TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#endif
#include "tiny_gltf.h"
#include <filesystem>

template <class T>
class ImageCPU {
 private:
  std::shared_ptr<T[]> _data;
  std::tuple<int, int> _resolution;
  int _channels;

 public:
  void setData(std::shared_ptr<T[]> data) { _data = data; }
  void setResolution(std::tuple<int, int> resolution) { _resolution = resolution; }
  void setChannels(int channels) { _channels = channels; }

  std::shared_ptr<T[]> getData() { return _data; }
  std::tuple<int, int> getResolution() { return _resolution; }
  int getChannels() { return _channels; }
};

class LoaderImage {
 private:
  std::shared_ptr<State> _state;

 public:
  LoaderImage(std::shared_ptr<State> state);
  template <class T>
  std::shared_ptr<ImageCPU<T>> loadCPU(std::string path);

  // have to support vector of inputs for cubemap
  template <class T>
  std::shared_ptr<BufferImage> loadGPU(std::vector<std::shared_ptr<ImageCPU<T>>> imagesCPU) {
    auto [width, height] = imagesCPU[0]->getResolution();
    int channels = imagesCPU[0]->getChannels();
    std::shared_ptr<BufferImage> bufferImage = std::make_shared<BufferImage>(
        std::tuple{width, height}, channels, imagesCPU.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);
    for (int i = 0; i < imagesCPU.size(); i++) {
      auto pixels = imagesCPU[i]->getData();
      VkDeviceSize imageSize = width * height * channels;
      bufferImage->setData(pixels.get(), imageSize * sizeof(T), imageSize * sizeof(T) * i);
    }

    return bufferImage;
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