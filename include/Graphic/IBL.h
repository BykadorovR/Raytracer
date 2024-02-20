#pragma once
#include "Drawable.h"
#include "State.h"
#include "Cubemap.h"
#include "Camera.h"
#include "Material.h"
#include "Mesh.h"
#include "LightManager.h"
#include "ResourceManager.h"

class IBL {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Mesh3D> _mesh3D;
  std::shared_ptr<Mesh2D> _mesh2D;
  std::vector<std::shared_ptr<UniformBuffer>> _cameraBufferCubemap;
  std::shared_ptr<UniformBuffer> _cameraBuffer;
  std::vector<std::shared_ptr<DescriptorSet>> _descriptorSetCameraCubemap;
  std::shared_ptr<DescriptorSet> _descriptorSetCamera;
  std::shared_ptr<Pipeline> _pipelineEquirectangular, _pipelineDiffuse, _pipelineSpecular, _pipelineSpecularBRDF;
  std::shared_ptr<Material> _material;
  std::shared_ptr<MaterialColor> _defaultMaterialColor;
  MaterialType _materialType = MaterialType::COLOR;
  std::shared_ptr<LightManager> _lightManager;
  glm::mat4 _model = glm::mat4(1.f);

  void _draw(int face,
             std::shared_ptr<Camera> camera,
             std::shared_ptr<CommandBuffer> commandBuffer,
             std::shared_ptr<Pipeline> pipeline);

 public:
  IBL(std::vector<VkFormat> renderFormat,
      VkCullModeFlags cullMode,
      std::shared_ptr<LightManager> lightManager,
      std::shared_ptr<CommandBuffer> commandBufferTransfer,
      std::shared_ptr<ResourceManager> resourceManager,
      std::shared_ptr<State> state);
  void setMaterial(std::shared_ptr<MaterialColor> material);
  void setModel(glm::mat4 model);

  void drawSpecularBRDF(std::tuple<int, int> resolution,
                        std::shared_ptr<Camera> camera,
                        std::shared_ptr<CommandBuffer> commandBuffer);
  void drawEquirectangular(int face, std::shared_ptr<Camera> camera, std::shared_ptr<CommandBuffer> commandBuffer);
  void drawDiffuse(int face, std::shared_ptr<Camera> camera, std::shared_ptr<CommandBuffer> commandBuffer);
  void drawSpecular(int face, int mipMap, std::shared_ptr<Camera> camera, std::shared_ptr<CommandBuffer> commandBuffer);
};