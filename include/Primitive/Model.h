#pragma once
#include "Device.h"
#include "Buffer.h"
#include "Shader.h"
#include "Pipeline.h"
#include "Command.h"
#include "Settings.h"
#include "Camera.h"
#include "LightManager.h"
#include "Logger.h"
#include "Loader.h"
#include "Animation.h"
#include "Material.h"

enum class ModelRenderMode { DIRECTIONAL, POINT, FULL };

class Model3D {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<LoggerCPU> _loggerCPU;

  std::vector<NodeGLTF*> _nodes;
  std::vector<std::vector<std::shared_ptr<UniformBuffer>>> _cameraUBODepth;
  std::shared_ptr<UniformBuffer> _cameraUBOFull;
  std::shared_ptr<CommandPool> _commandPool;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;
  std::vector<std::vector<std::shared_ptr<DescriptorSet>>> _descriptorSetCameraDepth;
  std::shared_ptr<DescriptorSet> _descriptorSetCameraFull;
  std::shared_ptr<DescriptorSet> _descriptorSetJointsDefault;
  std::shared_ptr<DescriptorPool> _descriptorPool;
  // used only for pipeline layout, not used for bind pipeline (layout is the same in every pipeline)
  std::shared_ptr<Texture> _stubTexture;
  std::shared_ptr<Texture> _stubTextureNormal;
  std::vector<std::shared_ptr<Buffer>> _defaultSSBO;
  std::shared_ptr<Camera> _camera;
  glm::mat4 _model = glm::mat4(1.f);
  bool _enableDepth = true;
  int _animationIndex = 0;
  bool _enableShadow = true;
  bool _enableLighting = true;
  std::shared_ptr<LightManager> _lightManager;
  std::shared_ptr<Animation> _animation;
  std::vector<std::shared_ptr<MaterialPhong>> _materials;
  std::shared_ptr<MaterialPhong> _defaultMaterial;
  std::vector<std::shared_ptr<Mesh3D>> _meshes;

  void _drawNode(int currentFrame,
                 std::shared_ptr<CommandBuffer> commandBuffer,
                 std::shared_ptr<Pipeline> pipeline,
                 std::shared_ptr<Pipeline> pipelineCullOff,
                 std::shared_ptr<DescriptorSet> cameraDS,
                 std::shared_ptr<UniformBuffer> cameraUBO,
                 glm::mat4 view,
                 glm::mat4 projection,
                 NodeGLTF* node);

 public:
  Model3D(std::vector<NodeGLTF*> nodes,
          std::vector<std::shared_ptr<Mesh3D>> meshes,
          std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
          std::shared_ptr<CommandBuffer> commandBufferTransfer,
          std::shared_ptr<State> state);
  void enableShadow(bool enable);
  void enableLighting(bool enable);
  void setCamera(std::shared_ptr<Camera> camera);

  void setMaterial(std::vector<std::shared_ptr<MaterialPhong>> materials);
  void setAnimation(std::shared_ptr<Animation> animation);
  void setModel(glm::mat4 model);
  void enableDepth(bool enable);
  bool isDepthEnabled();

  void draw(int currentFrame,
            std::shared_ptr<CommandBuffer> commandBuffer,
            std::shared_ptr<Pipeline> pipeline,
            std::shared_ptr<Pipeline> pipelineCullOff);
  void drawShadow(int currentFrame,
                  std::shared_ptr<CommandBuffer> commandBuffer,
                  std::shared_ptr<Pipeline> pipeline,
                  std::shared_ptr<Pipeline> pipelineCullOff,
                  int lightIndex,
                  glm::mat4 view,
                  glm::mat4 projection,
                  int face);
};