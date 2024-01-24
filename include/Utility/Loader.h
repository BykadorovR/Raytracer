#pragma once
#include "State.h"
#include "tiny_gltf.h"
#include "Material.h"
#include "Mesh.h"
#include <filesystem>
#include "ResourceManager.h"

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

class Loader {
 private:
  std::filesystem::path _path;
  std::shared_ptr<State> _state;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;
  std::shared_ptr<ResourceManager> _resourceManager;
  // pool of all used textures to conviniently take neccessary in materials
  tinygltf::Model _model;
  tinygltf::TinyGLTF _loader;
  std::vector<std::shared_ptr<Texture>> _textures;
  //
  std::vector<std::shared_ptr<NodeGLTF>> _nodes;
  std::vector<std::shared_ptr<MaterialGLTF>> _materials;
  std::vector<std::shared_ptr<MaterialPhong>> _materialsPhong;
  std::vector<std::shared_ptr<MaterialPBR>> _materialsPBR;
  std::vector<std::shared_ptr<SkinGLTF>> _skins;
  std::vector<std::shared_ptr<AnimationGLTF>> _animations;
  std::vector<std::shared_ptr<Mesh3D>> _meshes;

  std::shared_ptr<Texture> _loadTexture(int imageIndex, VkFormat format);
  void _loadMaterials();
  void _loadAnimations();
  void _loadSkins();
  void _generateTangent(std::vector<uint32_t>& indexes, std::vector<Vertex3D>& vertices);

  std::shared_ptr<NodeGLTF> _findNode(std::shared_ptr<NodeGLTF> parent, uint32_t index);
  std::shared_ptr<NodeGLTF> _nodeFromIndex(uint32_t index);
  void _loadNode(tinygltf::Node& input, std::shared_ptr<NodeGLTF> parent, uint32_t nodeIndex);

 public:
  Loader(std::string path,
         std::shared_ptr<CommandBuffer> commandBufferTransfer,
         std::shared_ptr<ResourceManager> resourceManager,
         std::shared_ptr<State> state);
  std::vector<std::shared_ptr<MaterialPhong>> getMaterialsPhong();
  std::vector<std::shared_ptr<MaterialPBR>> getMaterialsPBR();

  const std::vector<std::shared_ptr<SkinGLTF>>& getSkins();
  const std::vector<std::shared_ptr<AnimationGLTF>>& getAnimations();
  // one mesh - one node
  const std::vector<std::shared_ptr<NodeGLTF>>& getNodes();
  const std::vector<std::shared_ptr<Mesh3D>>& getMeshes();
};