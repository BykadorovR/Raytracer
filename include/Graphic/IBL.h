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
  std::shared_ptr<Pipeline> _pipelineDiffuse, _pipelineSpecular, _pipelineSpecularBRDF;
  std::shared_ptr<Material> _material;
  std::shared_ptr<LightManager> _lightManager;
  glm::mat4 _model = glm::mat4(1.f);
  std::shared_ptr<LoggerGPU> _loggerGPU;
  std::shared_ptr<Cubemap> _cubemapDiffuse, _cubemapSpecular;
  std::shared_ptr<Texture> _textureSpecularBRDF;
  std::shared_ptr<CameraOrtho> _cameraSpecularBRDF;
  std::shared_ptr<CameraFly> _camera;

  void _draw(int face,
             std::shared_ptr<Camera> camera,
             std::shared_ptr<CommandBuffer> commandBuffer,
             std::shared_ptr<Pipeline> pipeline);

 public:
  IBL(std::shared_ptr<LightManager> lightManager,
      std::shared_ptr<CommandBuffer> commandBufferTransfer,
      std::shared_ptr<ResourceManager> resourceManager,
      std::shared_ptr<State> state);
  void setMaterial(std::shared_ptr<MaterialColor> material);
  void setPosition(glm::vec3 position);
  std::shared_ptr<Cubemap> getCubemapDiffuse();
  std::shared_ptr<Cubemap> getCubemapSpecular();
  std::shared_ptr<Texture> getTextureSpecularBRDF();

  void drawSpecularBRDF(std::shared_ptr<CommandBuffer> commandBuffer);
  void drawDiffuse(std::shared_ptr<CommandBuffer> commandBuffer);
  void drawSpecular(std::shared_ptr<CommandBuffer> commandBuffer);
};