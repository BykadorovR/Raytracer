#pragma once
#include "Model.h"

class Model3DManager {
 private:
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayoutGraphic, _descriptorSetLayoutCamera;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayoutJoints;
  std::shared_ptr<Pipeline> _pipeline;
  std::shared_ptr<Pipeline> _pipelineGLTF;

  int _descriptorPoolSize = 100;
  int _modelsCreated = 0;
  std::vector<std::shared_ptr<DescriptorPool>> _descriptorPool;
  std::shared_ptr<CommandPool> _commandPool;
  std::shared_ptr<CommandBuffer> _commandBuffer;
  std::shared_ptr<Queue> _queue;
  std::shared_ptr<Device> _device;
  std::shared_ptr<Settings> _settings;

  std::vector<std::shared_ptr<Model>> _models;
  std::vector<std::shared_ptr<Model>> _modelsGLTF;

 public:
  Model3DManager(std::shared_ptr<CommandPool> commandPool,
                 std::shared_ptr<CommandBuffer> commandBuffer,
                 std::shared_ptr<Queue> queue,
                 std::shared_ptr<RenderPass> render,
                 std::shared_ptr<Device> device,
                 std::shared_ptr<Settings> settings);

  std::shared_ptr<ModelOBJ> createModel(std::string path);
  std::shared_ptr<ModelGLTF> createModelGLTF(std::string path);
  void registerModel(std::shared_ptr<Model> model);
  void registerModelGLTF(std::shared_ptr<Model> model);
  void unregisterModel(std::shared_ptr<Model> model);
  void draw(float frameTimer, int currentFrame);
  void drawGLTF(float frameTimer, int currentFrame);
};