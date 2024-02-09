#pragma once

#include "Drawable.h"
#include "Sprite.h"
#include "Mesh.h"

class SpriteManager : public IDrawable, public IShadowable {
 private:
  // position in vector is set number
  std::map<MaterialType, std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>>>
      _descriptorSetLayout;
  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> _descriptorSetLayoutNormal;
  std::map<MaterialType, std::shared_ptr<Pipeline>> _pipeline;
  std::map<MaterialType, std::shared_ptr<Pipeline>> _pipelineWireframe;
  std::shared_ptr<Pipeline> _pipelineNormal, _pipelineTangent;
  std::shared_ptr<Pipeline> _pipelineDirectional, _pipelinePoint;

  std::map<std::shared_ptr<Sprite>, std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>>>
      _descriptorSetLayoutCustom;
  std::map<std::shared_ptr<Sprite>, std::shared_ptr<Pipeline>> _pipelineCustom;

  int _spritesCreated = 0;
  std::vector<VkFormat> _renderFormat;
  std::shared_ptr<State> _state;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;
  std::shared_ptr<Camera> _camera;
  std::shared_ptr<LightManager> _lightManager;
  std::vector<std::shared_ptr<Sprite>> _sprites;
  std::shared_ptr<MaterialPhong> _defaultMaterialPhong;
  std::shared_ptr<MaterialColor> _defaultMaterialColor;
  std::shared_ptr<MaterialPBR> _defaultMaterialPBR;
  std::shared_ptr<Mesh2D> _defaultMesh;
  std::shared_ptr<ResourceManager> _resourceManager;

 public:
  SpriteManager(std::vector<VkFormat> renderFormat,
                std::shared_ptr<LightManager> lightManager,
                std::shared_ptr<CommandBuffer> commandBufferTransfer,
                std::shared_ptr<ResourceManager> resourceManager,
                std::shared_ptr<State> state);
  std::shared_ptr<Sprite> createSprite();
  // used for specular IBL (requires custom shader)
  std::shared_ptr<Sprite> createSprite(
      std::shared_ptr<Shader> shader,
      std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> layouts);
  void registerSprite(std::shared_ptr<Sprite> sprite);
  void unregisterSprite(std::shared_ptr<Sprite> sprite);
  void setCamera(std::shared_ptr<Camera> camera);
  void draw(std::tuple<int, int> resolution, std::shared_ptr<CommandBuffer> commandBuffer) override;
  void drawShadow(std::shared_ptr<CommandBuffer> commandBuffer,
                  LightType lightType,
                  int lightIndex,
                  int face = 0) override;
};