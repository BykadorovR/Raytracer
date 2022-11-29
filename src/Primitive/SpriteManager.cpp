#include "SpriteManager.h"

SpriteManager::SpriteManager(std::shared_ptr<Shader> shader,
                             std::shared_ptr<CommandPool> commandPool,
                             std::shared_ptr<CommandBuffer> commandBuffer,
                             std::shared_ptr<Queue> queue,
                             std::shared_ptr<RenderPass> render,
                             std::shared_ptr<Device> device,
                             std::shared_ptr<Settings> settings) {
  _commandPool = commandPool;
  _commandBuffer = commandBuffer;
  _queue = queue;
  _device = device;
  _settings = settings;

  _descriptorPool.push_back(std::make_shared<DescriptorPool>(_descriptorPoolSize, device));
  _descriptorSetLayout = std::make_shared<DescriptorSetLayout>(device);
  _descriptorSetLayout->createGraphic();
  _pipeline = std::make_shared<Pipeline>(shader, _descriptorSetLayout, device);
  _pipeline->createGraphic(render);
}

std::shared_ptr<Sprite> SpriteManager::createSprite(std::shared_ptr<Texture> texture) {
  if ((_spritesCreated * _settings->getMaxFramesInFlight()) >= _descriptorPoolSize * _descriptorPool.size()) {
    _descriptorPool.push_back(std::make_shared<DescriptorPool>(_descriptorPoolSize, _device));
  }
  _spritesCreated++;
  return std::make_shared<Sprite>(texture, _descriptorSetLayout, _pipeline, _descriptorPool.back(), _commandPool,
                                  _commandBuffer, _queue, _device, _settings);
}

void SpriteManager::registerSprite(std::shared_ptr<Sprite> sprite) { _sprites.push_back(sprite); }

void SpriteManager::unregisterSprite(std::shared_ptr<Sprite> sprite) {
  _sprites.erase(std::remove(_sprites.begin(), _sprites.end(), sprite), _sprites.end());
}

void SpriteManager::draw(int currentFrame) {
  vkCmdBindPipeline(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline->getPipeline());

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  std::tie(viewport.width, viewport.height) = _settings->getResolution();
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(_commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(viewport.width, viewport.height);
  vkCmdSetScissor(_commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  for (auto sprite : _sprites) {
    sprite->draw(currentFrame);
  }
}