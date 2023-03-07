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
  virtual void draw(int currentFrame) = 0;
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
    std::vector<NodeGLTF*> children;
    MeshGLTF mesh;
    glm::mat4 matrix;
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

  // Contains the texture for a single glTF image
  // Images may be reused by texture objects and are as such separated
  struct ImageGLTF {
    std::shared_ptr<Texture> texture;
  };

  // A glTF texture stores a reference to the image and a sampler
  // In this sample, we are only interested in the image
  struct TextureGLTF {
    int32_t imageIndex;
  };

  std::vector<ImageGLTF> _images;
  std::vector<TextureGLTF> _textures;
  std::vector<MaterialGLTF> _materials;
  std::vector<NodeGLTF*> _nodes;

  void _loadImages(tinygltf::Model& model);
  void _loadMaterials(tinygltf::Model& model);
  void _loadTextures(tinygltf::Model& model);
  void _loadNode(tinygltf::Node& node,
                 tinygltf::Model& model,
                 std::vector<uint32_t>& indexBuffer,
                 std::vector<Vertex3D>& vertexBuffer);

 public:
  ModelGLTF(std::string path);
  void draw(int currentFrame);
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

  void draw(int currentFrame);
};