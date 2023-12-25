#pragma once
#include "Model.h"
#include "Mesh.h"

class Model3DManager {
 private:
  // position in vector is set number
  std::map<MaterialType, std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>>>
      _descriptorSetLayout;
  std::map<MaterialType, std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>>>
      _descriptorSetLayoutNormal;
  std::map<MaterialType, std::shared_ptr<Pipeline>> _pipeline, _pipelineCullOff, _pipelineWireframe;
  std::map<MaterialType, std::shared_ptr<Pipeline>> _pipelineNormal, _pipelineNormalWireframe, _pipelineNormalCullOff;
  std::shared_ptr<Pipeline> _pipelineDirectional, _pipelinePoint;

  std::shared_ptr<MaterialPhong> _defaultMaterialPhong;
  std::shared_ptr<MaterialPBR> _defaultMaterialPBR;
  std::shared_ptr<Mesh3D> _mesh;
  std::shared_ptr<Animation> _defaultAnimation;

  std::shared_ptr<DescriptorPool> _descriptorPool;
  std::shared_ptr<LightManager> _lightManager;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;
  std::shared_ptr<State> _state;
  std::shared_ptr<Camera> _camera;
  std::vector<std::shared_ptr<Model3D>> _modelsGLTF;

 public:
  Model3DManager(std::vector<VkFormat> renderFormat,
                 std::shared_ptr<LightManager> lightManager,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<State> state);

  std::shared_ptr<Model3D> createModel3D(const std::vector<std::shared_ptr<NodeGLTF>>& nodes,
                                         const std::vector<std::shared_ptr<Mesh3D>>& meshes);
  void setCamera(std::shared_ptr<Camera> camera);
  void registerModel3D(std::shared_ptr<Model3D> model);
  void unregisterModel3D(std::shared_ptr<Model3D> model);
  void draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer, DrawType drawType = DrawType::FILL);
  void drawShadow(int currentFrame,
                  std::shared_ptr<CommandBuffer> commandBuffer,
                  LightType lightType,
                  int lightIndex,
                  int face = 0);
};