#pragma once
#include "State.h"
#include "tiny_obj_loader.h"
#include "tiny_gltf.h"
#include "Material.h"
#include "Mesh.h"

// TODO: pointers to shared_ptr

// A node represents an object in the glTF scene graph
struct NodeGLTF {
  NodeGLTF* parent;
  // node index
  uint32_t index;
  std::vector<NodeGLTF*> children;
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

  ~NodeGLTF() {
    for (auto& child : children) {
      delete child;
    }
  }
};

// TODO: remove, should be replaced by Phong + PBR, only base color can store here
//  A glTF material stores information in e.g. the texture that is attached to it and colors
struct MaterialGLTF {
  // PBR info
  glm::vec4 baseColorFactor = glm::vec4(1.0f);  // default [1,1,1,1]
  std::shared_ptr<Texture> baseColorTexture;
  double metallicFactor;   // default 1
  double roughnessFactor;  // default 1
  std::shared_ptr<Texture> metallicRoughnessTexture;

  std::shared_ptr<Texture> normalTexture;
  std::shared_ptr<Texture> occludedTexture;
  double occlusionStrength;  // occludedColor = lerp(color, color * <sampled occlusion
                             // texture value>, <occlusion strength>)
  std::shared_ptr<Texture> emissiveTexture;
  std::vector<double> emissiveFactor;  // length 3. default [0, 0, 0]

  bool alphaMask;      // we are interested in alphaMode = "MASK" string value only
  double alphaCutoff;  // default 0.5
  bool doubleSided;    // default false;
};

struct SkinGLTF {
  std::string name;
  std::vector<glm::mat4> inverseBindMatrices;
  std::vector<NodeGLTF*> joints;
};

struct AnimationSamplerGLTF {
  std::string interpolation;
  std::vector<float> inputs;
  std::vector<glm::vec4> outputsVec4;
};

struct AnimationChannelGLTF {
  std::string path;
  NodeGLTF* node;
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
  std::string _path;
  std::shared_ptr<State> _state;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;

  std::vector<NodeGLTF*> _nodes;
  // pool of all used textures to conviniently take neccessary in materials
  tinygltf::Model _model;
  tinygltf::TinyGLTF _loader;
  std::vector<std::shared_ptr<Texture>> _textures;
  //
  std::vector<MaterialGLTF> _materials;
  std::vector<std::shared_ptr<MaterialPhong>> _materialsPhong;

  std::vector<SkinGLTF> _skins;
  std::vector<AnimationGLTF> _animations;
  std::vector<std::shared_ptr<Mesh3D>> _meshes;

  void _loadTextures();
  void _loadMaterials();
  void _loadAnimations();
  void _loadSkins();

  NodeGLTF* _findNode(NodeGLTF* parent, uint32_t index);
  NodeGLTF* _nodeFromIndex(uint32_t index);
  void _loadNode(tinygltf::Node& input, NodeGLTF* parent, uint32_t nodeIndex);

 public:
  Loader(std::string path, std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state);
  std::vector<std::shared_ptr<MaterialPhong>> getMaterialsPhong();

  const std::vector<SkinGLTF>& getSkins();
  const std::vector<AnimationGLTF>& getAnimations();
  // one mesh - one node
  std::vector<NodeGLTF*> getNodes();
  std::vector<std::shared_ptr<Mesh3D>> getMeshes();
};