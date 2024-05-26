#pragma once

#include "State.h"
#include "Camera.h"
#include "Mesh.h"
#include "Descriptor.h"
#include "Pipeline.h"
#include "Drawable.h"
#include "Material.h"
#include "ResourceManager.h"

enum class ShapeType { CUBE = 0, SPHERE = 1 };

class Shape3D : public Drawable, public Shadowable {
 private:
  std::map<ShapeType, std::map<MaterialType, std::vector<std::string>>> _shadersColor;
  std::map<ShapeType, std::vector<std::string>> _shadersLight, _shadersNormalsMesh, _shadersTangentMesh;
  ShapeType _shapeType;
  std::shared_ptr<State> _state;
  std::shared_ptr<Mesh3D> _mesh;
  std::map<MaterialType, std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>>>
      _descriptorSetLayout;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayoutNormalsMesh;
  std::shared_ptr<DescriptorSet> _descriptorSetNormalsMesh, _descriptorSetColor, _descriptorSetPhong, _descriptorSetPBR;
  std::shared_ptr<UniformBuffer> _uniformBufferCamera;

  std::vector<std::vector<std::shared_ptr<UniformBuffer>>> _cameraUBODepth;
  std::vector<std::vector<std::shared_ptr<DescriptorSet>>> _descriptorSetCameraDepth;
  std::map<ShapeType, std::map<MaterialType, std::shared_ptr<Pipeline>>> _pipeline, _pipelineWireframe;
  std::shared_ptr<RenderPass> _renderPass, _renderPassDepth;
  std::shared_ptr<Pipeline> _pipelineDirectional, _pipelinePoint, _pipelineNormalMesh, _pipelineTangentMesh;
  std::shared_ptr<Material> _material;
  std::shared_ptr<MaterialColor> _defaultMaterialColor;
  std::shared_ptr<MaterialPhong> _defaultMaterialPhong;
  std::shared_ptr<MaterialPBR> _defaultMaterialPBR;
  std::shared_ptr<LightManager> _lightManager;
  MaterialType _materialType = MaterialType::COLOR;
  DrawType _drawType = DrawType::FILL;
  VkCullModeFlags _cullMode;
  bool _enableShadow = true;
  bool _enableLighting = true;

  void _updateColorDesctiptor(std::shared_ptr<MaterialColor> material);
  void _updatePhongDesctiptor(std::shared_ptr<MaterialPhong> material);
  void _updatePBRDesctiptor(std::shared_ptr<MaterialPBR> material);

 public:
  Shape3D(ShapeType shapeType,
          VkCullModeFlags cullMode,
          std::shared_ptr<LightManager> lightManager,
          std::shared_ptr<CommandBuffer> commandBufferTransfer,
          std::shared_ptr<ResourceManager> resourceManager,
          std::shared_ptr<State> state);

  void enableShadow(bool enable);
  void enableLighting(bool enable);
  void setMaterial(std::shared_ptr<MaterialColor> material);
  void setMaterial(std::shared_ptr<MaterialPhong> material);
  void setMaterial(std::shared_ptr<MaterialPBR> material);
  void setDrawType(DrawType drawType);

  std::shared_ptr<Mesh3D> getMesh();

  void draw(std::tuple<int, int> resolution,
            std::shared_ptr<Camera> camera,
            std::shared_ptr<CommandBuffer> commandBuffer) override;
  void drawShadow(LightType lightType, int lightIndex, int face, std::shared_ptr<CommandBuffer> commandBuffer) override;
};