#pragma once

#include "Sprite.h"
#include "Mesh.h"

class SpriteManager {
 private:
  // position in vector is set number
  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> _descriptorSetLayout;
  std::map<SpriteRenderMode, std::shared_ptr<Pipeline>> _pipeline;

  int _spritesCreated = 0;
  std::shared_ptr<State> _state;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;
  std::shared_ptr<Camera> _camera;
  std::shared_ptr<LightManager> _lightManager;
  std::vector<std::shared_ptr<Sprite>> _sprites;
  std::shared_ptr<MaterialPhong> _defaultMaterial;
  std::shared_ptr<Mesh2D> _defaultMesh;

 public:
  SpriteManager(std::vector<VkFormat> renderFormat,
                std::shared_ptr<LightManager> lightManager,
                std::shared_ptr<CommandBuffer> commandBufferTransfer,
                std::shared_ptr<State> state);
  std::shared_ptr<Sprite> createSprite();
  std::shared_ptr<Sprite> createSprite(std::shared_ptr<MaterialPhong> material);
  void registerSprite(std::shared_ptr<Sprite> sprite);
  void unregisterSprite(std::shared_ptr<Sprite> sprite);
  void setCamera(std::shared_ptr<Camera> camera);
  void draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer);
  void drawShadow(int currentFrame,
                  std::shared_ptr<CommandBuffer> commandBuffer,
                  LightType lightType,
                  int lightIndex,
                  int face = 0);
};