#pragma once
#include "Device.h"
#include "Buffer.h"
#include "Shader.h"
#include "Render.h"
#include "Pipeline.h"
#include "Command.h"
#include "Queue.h"
#include "Settings.h"
#include "tiny_obj_loader.h"
#include "tiny_gltf.h"

class Model {
 public:
  virtual void draw(float frameTimer, int currentFrame) = 0;
};

class ModelOBJ : public Model {
 private:
  std::shared_ptr<Settings> _settings;
  std::shared_ptr<Device> _device;
  std::shared_ptr<Pipeline> _pipeline;
  std::shared_ptr<CommandPool> _commandPool;
  std::shared_ptr<DescriptorSet> _descriptorSet;
  std::shared_ptr<CommandBuffer> _commandBuffer;
  std::shared_ptr<Texture> _texture;

  std::shared_ptr<VertexBuffer3D> _vertexBuffer;
  std::shared_ptr<IndexBuffer> _indexBuffer;
  std::shared_ptr<UniformBuffer> _uniformBuffer;

  glm::mat4 _model, _view, _projection;
  std::string _path;

  std::vector<Vertex3D> _vertices;
  std::vector<uint32_t> _indices;

  void _loadModel();

 public:
  ModelOBJ(std::string path,
           std::shared_ptr<Texture> texture,
           std::shared_ptr<DescriptorSetLayout> descriptorSetLayout,
           std::shared_ptr<Pipeline> pipeline,
           std::shared_ptr<DescriptorPool> descriptorPool,
           std::shared_ptr<CommandPool> commandPool,
           std::shared_ptr<CommandBuffer> commandBuffer,
           std::shared_ptr<Queue> queue,
           std::shared_ptr<Device> device,
           std::shared_ptr<Settings> settings);

  void setModel(glm::mat4 model);
  void setView(glm::mat4 view);
  void setProjection(glm::mat4 projection);

  void draw(float frameTimer, int currentFrame);
};

class ModelGLTF : public Model {
 private:
  // A primitive contains the data for a single draw call
  struct PrimitiveGLTF {
    uint32_t firstIndex;
    uint32_t indexCount;
    int32_t materialIndex;
  };

  // Contains the node's (optional) geometry and can be made up of an arbitrary number of primitives
  struct MeshGLTF {
    std::vector<PrimitiveGLTF> primitives;
  };

  // A node represents an object in the glTF scene graph
  struct NodeGLTF {
    NodeGLTF* parent;
    // node index
    uint32_t index;
    std::vector<NodeGLTF*> children;
    MeshGLTF mesh;

    glm::vec3 translation{};
    glm::vec3 scale{1.0f};
    glm::quat rotation{};
    glm::mat4 matrix;

    int32_t skin = -1;

    /* Get a node's local matrix from the current translation, rotation and scale values
       These are calculated from the current animation and need to be calculated dynamically*/
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

  // A glTF material stores information in e.g. the texture that is attached to it and colors
  struct MaterialGLTF {
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    uint32_t baseColorTextureIndex;
  };

  // Images may be reused by texture objects and are as such separated
  struct ImageGLTF {
    std::shared_ptr<Texture> texture;
    std::shared_ptr<DescriptorSet> descriptorSet;
  };

  // A glTF texture stores a reference to the image and a sampler
  // In this sample, we are only interested in the image
  struct TextureGLTF {
    int32_t imageIndex;
  };

  struct SkinGLTF {
    std::string name;
    NodeGLTF* skeletonRoot = nullptr;
    std::vector<glm::mat4> inverseBindMatrices;
    std::vector<NodeGLTF*> joints;
    std::shared_ptr<Buffer> ssbo;
    std::shared_ptr<DescriptorSet> descriptorSet;
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

  std::vector<ImageGLTF> _images;
  std::vector<TextureGLTF> _textures;
  std::vector<MaterialGLTF> _materials;
  std::vector<SkinGLTF> _skins;
  std::vector<AnimationGLTF> _animations;
  uint32_t _activeAnimation = 0;

  std::vector<NodeGLTF*> _nodes;
  glm::mat4 _model;
  glm::mat4 _view;
  glm::mat4 _projection;

  std::shared_ptr<UniformBuffer> _uniformBuffer;
  std::shared_ptr<VertexBuffer3D> _vertexBuffer;
  std::shared_ptr<IndexBuffer> _indexBuffer;
  std::shared_ptr<CommandPool> _commandPool;
  std::shared_ptr<CommandBuffer> _commandBuffer;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayout;
  std::shared_ptr<DescriptorSet> _descriptorSet;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayoutJoints;
  std::shared_ptr<DescriptorPool> _descriptorPool;
  std::shared_ptr<Pipeline> _pipeline;
  std::shared_ptr<Queue> _queue;
  std::shared_ptr<Device> _device;

  void _updateAnimation(float deltaTime);
  void _updateJoints(NodeGLTF* node);
  glm::mat4 _getNodeMatrix(NodeGLTF* node);
  void _loadAnimations(tinygltf::Model& model);
  void _loadSkins(tinygltf::Model& model);
  void _loadImages(tinygltf::Model& model);
  void _loadMaterials(tinygltf::Model& model);
  void _loadTextures(tinygltf::Model& model);

  NodeGLTF* _findNode(NodeGLTF* parent, uint32_t index);
  ModelGLTF::NodeGLTF* _nodeFromIndex(uint32_t index);
  void _loadNode(tinygltf::Node& input,
                 tinygltf::Model& model,
                 NodeGLTF* parent,
                 uint32_t nodeIndex,
                 std::vector<uint32_t>& indexBuffer,
                 std::vector<Vertex3D>& vertexBuffer);
  void _drawNode(int currentFrame, NodeGLTF* node);

 public:
  ModelGLTF(std::string path,
            std::shared_ptr<DescriptorSetLayout> descriptorSetLayout,
            std::shared_ptr<DescriptorSetLayout> descriptorSetLayoutJoints,
            std::shared_ptr<Pipeline> pipeline,
            std::shared_ptr<DescriptorPool> descriptorPool,
            std::shared_ptr<CommandPool> commandPool,
            std::shared_ptr<CommandBuffer> commandBuffer,
            std::shared_ptr<Queue> queue,
            std::shared_ptr<Device> device,
            std::shared_ptr<Settings> settings);

  void setModel(glm::mat4 model);
  void setView(glm::mat4 view);
  void setProjection(glm::mat4 projection);

  void draw(float frameTimer, int currentFrame);
};