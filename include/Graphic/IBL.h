#pragma once
#include "Drawable.h"
#include "State.h"
#include "Cubemap.h"
#include "Camera.h"
#include "Material.h"
#include "Mesh.h"
#include "LightManager.h"

class IBL {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Mesh3D> _mesh;
  std::vector<std::shared_ptr<UniformBuffer>> _uniformBuffer;
  std::vector<std::shared_ptr<DescriptorSet>> _descriptorSetCamera;
  std::shared_ptr<Pipeline> _pipelineEquirectangular, _pipelineDiffuse, _pipelineSpecular;
  std::shared_ptr<Camera> _camera;
  std::shared_ptr<Material> _material;
  std::shared_ptr<MaterialColor> _defaultMaterialColor;
  MaterialType _materialType = MaterialType::COLOR;
  std::shared_ptr<LightManager> _lightManager;
  glm::mat4 _model = glm::mat4(1.f);

  void _draw(int currentFrame,
             std::shared_ptr<Pipeline> pipeline,
             std::shared_ptr<CommandBuffer> commandBuffer,
             int face = 0);

 public:
  IBL(std::vector<VkFormat> renderFormat,
      VkCullModeFlags cullMode,
      std::shared_ptr<LightManager> lightManager,
      std::shared_ptr<CommandBuffer> commandBufferTransfer,
      std::shared_ptr<State> state);
  void setMaterial(std::shared_ptr<MaterialColor> material);
  void setModel(glm::mat4 model);
  void setCamera(std::shared_ptr<Camera> camera);

  void drawEquirectangular(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer, int face);
  void drawDiffuse(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer, int face);
  void drawSpecular(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer, int face, int mipMap);
};