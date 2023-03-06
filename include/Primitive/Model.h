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

  std::shared_ptr<VertexBuffer> _vertexBuffer;
  std::shared_ptr<IndexBuffer> _indexBuffer;
  std::shared_ptr<UniformBuffer> _uniformBuffer;

  glm::mat4 _model, _view, _projection;
  std::string _path;

  std::vector<Vertex> _vertices;
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