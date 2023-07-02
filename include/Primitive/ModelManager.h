#pragma once
#include "Model.h"
#include "LightManager.h"
#include "Camera.h"

class Model3DManager {
 private:
  // position in vector is set number
  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> _descriptorSetLayout;
  std::map<ModelRenderMode, std::shared_ptr<Pipeline>> _pipeline;
  std::map<ModelRenderMode, std::shared_ptr<Pipeline>> _pipelineCullOff;

  std::shared_ptr<DescriptorPool> _descriptorPool;
  std::shared_ptr<LightManager> _lightManager;
  std::shared_ptr<CommandPool> _commandPool;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;
  std::shared_ptr<Device> _device;
  std::shared_ptr<Settings> _settings;
  std::shared_ptr<Camera> _camera;
  std::vector<std::shared_ptr<Model>> _modelsGLTF;
  std::shared_ptr<RenderPass> _renderPass;

 public:
  Model3DManager(std::shared_ptr<LightManager> lightManager,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<DescriptorPool> descriptorPool,
                 std::shared_ptr<RenderPass> render,
                 std::shared_ptr<RenderPass> renderDepth,
                 std::shared_ptr<Device> device,
                 std::shared_ptr<Settings> settings);

  std::shared_ptr<ModelGLTF> createModelGLTF(std::string path);
  void setCamera(std::shared_ptr<Camera> camera);
  void registerModelGLTF(std::shared_ptr<Model> model);
  void unregisterModelGLTF(std::shared_ptr<Model> model);
  void draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer);
  void drawShadow(int currentFrame,
                  std::shared_ptr<CommandBuffer> commandBuffer,
                  LightType lightType,
                  int lightIndex,
                  int face = 0);
  void updateAnimation(float deltaTime);
};