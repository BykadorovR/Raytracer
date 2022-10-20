#pragma once

#include "Sprite.h"

class SpriteManager {
 private:
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayout;
  std::shared_ptr<Pipeline> _pipeline;

  int _descriptorPoolSize = 100;
  int _spritesCreated = 0;
  std::vector<std::shared_ptr<DescriptorPool>> _descriptorPool;
  std::shared_ptr<CommandPool> _commandPool;
  std::shared_ptr<CommandBuffer> _commandBuffer;
  std::shared_ptr<Queue> _queue;
  std::shared_ptr<Device> _device;
  std::shared_ptr<Settings> _settings;

  std::vector<std::shared_ptr<Sprite>> _sprites;

 public:
  SpriteManager(std::shared_ptr<CommandPool> commandPool,
                std::shared_ptr<CommandBuffer> commandBuffer,
                std::shared_ptr<Queue> queue,
                std::shared_ptr<RenderPass> render,
                std::shared_ptr<Device> device,
                std::shared_ptr<Settings> settings);
  std::shared_ptr<Sprite> createSprite();
  void registerSprite(std::shared_ptr<Sprite> sprite);
  void unregisterSprite(std::shared_ptr<Sprite> sprite);
  void draw(int currentFrame);
};