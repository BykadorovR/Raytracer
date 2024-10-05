#pragma once
#include "Utility/State.h"
#include "Utility/GameState.h"
#include "Vulkan/Render.h"
#include "Primitive/Drawable.h"
#include "Primitive/Cubemap.h"
#include "Primitive/Mesh.h"
#include "Graphic/Camera.h"
#include "Graphic/Material.h"

class IBL {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<GameState> _gameState;
  std::shared_ptr<MeshStatic3D> _mesh3D;
  std::shared_ptr<MeshStatic2D> _mesh2D;
  std::vector<std::shared_ptr<UniformBuffer>> _cameraBufferCubemap;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;
  std::shared_ptr<UniformBuffer> _cameraBuffer;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayoutColor;
  std::vector<std::shared_ptr<DescriptorSet>> _descriptorSetColor;
  std::shared_ptr<DescriptorSet> _descriptorSetBRDF;
  std::shared_ptr<Pipeline> _pipelineDiffuse, _pipelineSpecular, _pipelineSpecularBRDF;
  std::shared_ptr<MaterialColor> _material;
  glm::mat4 _model = glm::mat4(1.f);
  std::shared_ptr<Logger> _logger;
  std::shared_ptr<Cubemap> _cubemapDiffuse, _cubemapSpecular;
  std::shared_ptr<Texture> _textureSpecularBRDF;
  std::shared_ptr<CameraOrtho> _cameraSpecularBRDF;
  std::shared_ptr<CameraFly> _camera;

  std::shared_ptr<RenderPass> _renderPass;
  // we do it once, so we don't need max frames in flight
  std::vector<std::vector<std::shared_ptr<Framebuffer>>> _frameBufferSpecular;
  std::vector<std::shared_ptr<Framebuffer>> _frameBufferDiffuse;
  std::shared_ptr<Framebuffer> _frameBufferBRDF;

  void _updateColorDescriptor(std::shared_ptr<MaterialColor> material);
  void _draw(int face,
             std::shared_ptr<Camera> camera,
             std::shared_ptr<CommandBuffer> commandBuffer,
             std::shared_ptr<Pipeline> pipeline);

 public:
  IBL(std::shared_ptr<CommandBuffer> commandBufferTransfer,
      std::shared_ptr<GameState> gameState,
      std::shared_ptr<State> state);
  void setMaterial(std::shared_ptr<MaterialColor> material);
  void setPosition(glm::vec3 position);
  std::shared_ptr<Cubemap> getCubemapDiffuse();
  std::shared_ptr<Cubemap> getCubemapSpecular();
  std::shared_ptr<Texture> getTextureSpecularBRDF();
  void drawSpecularBRDF();
  void drawDiffuse();
  void drawSpecular();
};