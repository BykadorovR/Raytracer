#pragma once

#include "Sprite.h"

class SpriteManager {
 private:
  // position in vector is set number
  std::vector<std::shared_ptr<DescriptorSetLayout>> _descriptorSetLayout;
  std::map<SpriteRenderMode, std::shared_ptr<Pipeline>> _pipeline;

  int _spritesCreated = 0;
  std::shared_ptr<DescriptorPool> _descriptorPool;
  std::shared_ptr<CommandPool> _commandPool;
  std::shared_ptr<CommandBuffer> _commandBuffer;
  std::shared_ptr<Queue> _queue;
  std::shared_ptr<Device> _device;
  std::shared_ptr<Settings> _settings;
  std::shared_ptr<Camera> _camera;
  std::shared_ptr<CameraOrtho> _cameraOrtho;
  std::shared_ptr<LightManager> _lightManager;
  std::shared_ptr<UniformBuffer> _uniformShadow;
  std::shared_ptr<DescriptorSet> _shadowDescriptorSet;
  std::shared_ptr<DescriptorSetLayout> _shadowDescriptorSetLayout;
  std::vector<std::shared_ptr<Sprite>> _sprites;

 public:
  SpriteManager(std::shared_ptr<LightManager> lightManager,
                std::shared_ptr<CommandPool> commandPool,
                std::shared_ptr<CommandBuffer> commandBuffer,
                std::shared_ptr<Queue> queue,
                std::shared_ptr<DescriptorPool> descriptorPool,
                std::shared_ptr<RenderPass> render,
                std::shared_ptr<RenderPass> renderDepth,
                std::shared_ptr<Device> device,
                std::shared_ptr<Settings> settings);
  std::shared_ptr<Sprite> createSprite(std::shared_ptr<Texture> texture,
                                       std::shared_ptr<Texture> normalMap,
                                       std::shared_ptr<Texture> shadowMap);
  void registerSprite(std::shared_ptr<Sprite> sprite);
  void unregisterSprite(std::shared_ptr<Sprite> sprite);
  void setCamera(std::shared_ptr<Camera> camera);
  void draw(int currentFrame, SpriteRenderMode mode);
};