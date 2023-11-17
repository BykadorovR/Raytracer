#pragma once
#include "State.h"
#include "Cubemap.h"
#include "Camera.h"
#include "Material.h"
#include "Mesh.h"
#include "LightManager.h"

class Cube {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Mesh3D> _mesh;
  std::vector<std::shared_ptr<UniformBuffer>> _uniformBuffer;
  std::vector<std::shared_ptr<DescriptorSet>> _descriptorSetCamera;
  std::vector<std::vector<std::shared_ptr<DescriptorSet>>> _descriptorSetCameraDepth;
  std::shared_ptr<Pipeline> _pipeline, _pipelineEquirectangular, _pipelineDirectional, _pipelinePoint;
  std::shared_ptr<Camera> _camera;
  std::shared_ptr<Material> _material;
  std::shared_ptr<MaterialColor> _defaultMaterialColor;
  MaterialType _materialType = MaterialType::COLOR;
  std::vector<std::vector<std::shared_ptr<UniformBuffer>>> _cameraUBODepth;
  std::shared_ptr<LightManager> _lightManager;
  glm::mat4 _model = glm::mat4(1.f);

  void _draw(int currentFrame,
             std::shared_ptr<Pipeline> pipeline,
             std::shared_ptr<CommandBuffer> commandBuffer,
             int face = 0);

 public:
  Cube(std::vector<VkFormat> renderFormat,
       VkCullModeFlags cullMode,
       VkPolygonMode polygonMode,
       std::shared_ptr<LightManager> lightManager,
       std::shared_ptr<CommandBuffer> commandBufferTransfer,
       std::shared_ptr<State> state);
  void setMaterial(std::shared_ptr<MaterialColor> material);
  void setModel(glm::mat4 model);
  void setCamera(std::shared_ptr<Camera> camera);
  std::shared_ptr<Mesh3D> getMesh();

  void draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer);
  void drawEquirectangular(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer, int face);
  void drawShadow(int currentFrame,
                  std::shared_ptr<CommandBuffer> commandBuffer,
                  LightType lightType,
                  int lightIndex,
                  int face = 0);
};