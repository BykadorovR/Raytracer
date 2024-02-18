#pragma once
#include "Drawable.h"
#include "Model.h"
#include "Mesh.h"

class Model3DManager : public IDrawable, public IShadowable {
 private:
  // position in vector is set number
  std::map<MaterialType, std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>>>
      _descriptorSetLayout;
  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> _descriptorSetLayoutNormal;
  std::map<MaterialType, std::shared_ptr<Pipeline>> _pipeline, _pipelineCullOff, _pipelineWireframe;
  std::shared_ptr<Pipeline> _pipelineNormalMesh, _pipelineNormalMeshCullOff, _pipelineTangentMesh,
      _pipelineTangentMeshCullOff;
  std::shared_ptr<Pipeline> _pipelineDirectional, _pipelinePoint;

  std::shared_ptr<MaterialPhong> _defaultMaterialPhong;
  std::shared_ptr<MaterialPBR> _defaultMaterialPBR;
  std::shared_ptr<MaterialColor> _defaultMaterialColor;
  std::shared_ptr<Mesh3D> _mesh;
  std::shared_ptr<Animation> _defaultAnimation;

  std::shared_ptr<DescriptorPool> _descriptorPool;
  std::shared_ptr<LightManager> _lightManager;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;
  std::shared_ptr<State> _state;
  std::vector<std::shared_ptr<Model3D>> _modelsGLTF;
  std::shared_ptr<ResourceManager> _resourceManager;

 public:
  Model3DManager(std::vector<VkFormat> renderFormat,
                 std::shared_ptr<LightManager> lightManager,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<ResourceManager> resourceManager,
                 std::shared_ptr<State> state);

  std::shared_ptr<Model3D> createModel3D(const std::vector<std::shared_ptr<NodeGLTF>>& nodes,
                                         const std::vector<std::shared_ptr<Mesh3D>>& meshes);
  void registerModel3D(std::shared_ptr<Model3D> model);
  void unregisterModel3D(std::shared_ptr<Model3D> model);
  void draw(std::tuple<int, int> resolution,
            std::shared_ptr<Camera> camera,
            std::shared_ptr<CommandBuffer> commandBuffer) override;
  void drawShadow(LightType lightType, int lightIndex, int face, std::shared_ptr<CommandBuffer> commandBuffer) override;
};