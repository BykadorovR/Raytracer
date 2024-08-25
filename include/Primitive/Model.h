#pragma once
#include "Vulkan/Device.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/Shader.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/Command.h"
#include "Utility/Settings.h"
#include "Utility/ResourceManager.h"
#include "Utility/Logger.h"
#include "Utility/Loader.h"
#include "Utility/Animation.h"
#include "Graphic/Camera.h"
#include "Graphic/LightManager.h"
#include "Graphic/Material.h"
#include "Primitive/Drawable.h"

class Model3D : public Drawable, public Shadowable {
 private:
  std::shared_ptr<State> _state;

  std::vector<std::shared_ptr<NodeGLTF>> _nodes;
  std::vector<std::vector<std::shared_ptr<UniformBuffer>>> _cameraUBODepth;
  std::shared_ptr<UniformBuffer> _cameraUBOFull;
  std::vector<std::vector<std::shared_ptr<DescriptorSet>>> _descriptorSetCameraDepth;
  std::vector<std::shared_ptr<DescriptorSet>> _descriptorSetColor, _descriptorSetPhong, _descriptorSetPBR,
      _descriptorSetJoints;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayoutJoints;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayoutNormalsMesh;
  std::shared_ptr<DescriptorSet> _descriptorSetNormalsMesh;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayoutColor, _descriptorSetLayoutPhong, _descriptorSetLayoutPBR;
  std::map<MaterialType, std::shared_ptr<Pipeline>> _pipeline, _pipelineCullOff, _pipelineWireframe;
  std::shared_ptr<RenderPass> _renderPass, _renderPassDepth;
  std::shared_ptr<Pipeline> _pipelineNormalMesh, _pipelineNormalMeshCullOff, _pipelineTangentMesh,
      _pipelineTangentMeshCullOff;
  std::shared_ptr<Pipeline> _pipelineDirectional, _pipelinePoint;

  std::shared_ptr<MaterialPhong> _defaultMaterialPhong;
  std::shared_ptr<MaterialPBR> _defaultMaterialPBR;
  std::shared_ptr<MaterialColor> _defaultMaterialColor;
  std::shared_ptr<Mesh3D> _mesh;
  std::shared_ptr<Animation> _defaultAnimation;
  bool _enableDepth = true;
  bool _enableShadow = true;
  bool _enableLighting = true;
  std::shared_ptr<LightManager> _lightManager;
  std::shared_ptr<Animation> _animation;
  std::vector<std::shared_ptr<Material>> _materials;
  std::vector<std::shared_ptr<Mesh3D>> _meshes;
  MaterialType _materialType = MaterialType::PHONG;
  DrawType _drawType = DrawType::FILL;

  void _updateJointsDescriptor();
  void _updateColorDescriptor(std::vector<std::shared_ptr<MaterialColor>> materials);
  void _updatePhongDescriptor(std::vector<std::shared_ptr<MaterialPhong>> materials);
  void _updatePBRDescriptor(std::vector<std::shared_ptr<MaterialPBR>> materials);

  void _drawNode(std::shared_ptr<CommandBuffer> commandBuffer,
                 std::shared_ptr<Pipeline> pipeline,
                 std::shared_ptr<Pipeline> pipelineCullOff,
                 std::shared_ptr<DescriptorSet> cameraDS,
                 std::shared_ptr<UniformBuffer> cameraUBO,
                 glm::mat4 view,
                 glm::mat4 projection,
                 std::shared_ptr<NodeGLTF> node);

 public:
  Model3D(const std::vector<std::shared_ptr<NodeGLTF>>& nodes,
          const std::vector<std::shared_ptr<Mesh3D>>& meshes,
          std::shared_ptr<LightManager> lightManager,
          std::shared_ptr<CommandBuffer> commandBufferTransfer,
          std::shared_ptr<ResourceManager> resourceManager,
          std::shared_ptr<State> state);
  void enableShadow(bool enable);
  void enableLighting(bool enable);

  void setMaterial(std::vector<std::shared_ptr<MaterialPBR>> materials);
  void setMaterial(std::vector<std::shared_ptr<MaterialPhong>> materials);
  void setMaterial(std::vector<std::shared_ptr<MaterialColor>> materials);
  void setAnimation(std::shared_ptr<Animation> animation);
  void setDrawType(DrawType drawType);

  void enableDepth(bool enable);
  bool isDepthEnabled();

  MaterialType getMaterialType();
  DrawType getDrawType();

  void draw(std::tuple<int, int> resolution,
            std::shared_ptr<Camera> camera,
            std::shared_ptr<CommandBuffer> commandBuffer) override;
  void drawShadow(LightType lightType, int lightIndex, int face, std::shared_ptr<CommandBuffer> commandBuffer) override;
};