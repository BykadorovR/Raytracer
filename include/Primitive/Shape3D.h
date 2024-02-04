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

class Shape3D : public IDrawable, IShadowable {
 private:
  std::map<ShapeType, std::map<MaterialType, std::vector<std::string>>> _shadersColor;
  std::map<ShapeType, std::vector<std::string>> _shadersLight, _shadersNormalsMesh, _shadersTangentMesh;
  ShapeType _shapeType;
  std::shared_ptr<State> _state;
  std::shared_ptr<Mesh3D> _mesh;
  std::map<MaterialType, std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>>>
      _descriptorSetLayout;
  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> _descriptorSetLayoutNormalsMesh;
  std::shared_ptr<UniformBuffer> _uniformBufferCamera;
  std::shared_ptr<DescriptorSet> _descriptorSetCamera, _descriptorSetCameraGeometry;
  std::vector<std::vector<std::shared_ptr<UniformBuffer>>> _cameraUBODepth;
  std::vector<std::vector<std::shared_ptr<DescriptorSet>>> _descriptorSetCameraDepth;
  std::map<MaterialType, std::shared_ptr<Pipeline>> _pipeline;
  std::map<MaterialType, std::shared_ptr<Pipeline>> _pipelineWireframe;
  std::shared_ptr<Pipeline> _pipelineDirectional, _pipelinePoint, _pipelineNormalMesh, _pipelineTangentMesh;
  std::shared_ptr<Camera> _camera;
  std::shared_ptr<Material> _material;
  std::shared_ptr<MaterialColor> _defaultMaterialColor;
  std::shared_ptr<MaterialPhong> _defaultMaterialPhong;
  std::shared_ptr<MaterialPBR> _defaultMaterialPBR;
  std::shared_ptr<LightManager> _lightManager;
  MaterialType _materialType = MaterialType::COLOR;
  glm::mat4 _model = glm::mat4(1.f);
  DrawType _drawType = DrawType::FILL;
  bool _enableShadow = true;
  bool _enableLighting = true;

 public:
  Shape3D(ShapeType shapeType,
          std::vector<VkFormat> renderFormat,
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

  void setModel(glm::mat4 model);
  void setCamera(std::shared_ptr<Camera> camera);
  std::shared_ptr<Mesh3D> getMesh();

  void draw(int currentFrame, std::tuple<int, int> resolution, std::shared_ptr<CommandBuffer> commandBuffer) override;
  void drawShadow(int currentFrame,
                  std::shared_ptr<CommandBuffer> commandBuffer,
                  LightType lightType,
                  int lightIndex,
                  int face = 0) override;
};