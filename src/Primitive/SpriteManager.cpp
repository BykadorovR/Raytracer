#include "SpriteManager.h"

SpriteManager::SpriteManager(std::shared_ptr<Shader> shader,
                             std::shared_ptr<LightManager> lightManager,
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
  _lightManager = lightManager;

  _descriptorPool.push_back(std::make_shared<DescriptorPool>(_descriptorPoolSize, device));
  _descriptorSetLayoutGraphic = std::make_shared<DescriptorSetLayout>(device);
  _descriptorSetLayoutGraphic->createGraphicModel();

  _descriptorSetLayoutCamera = std::make_shared<DescriptorSetLayout>(device);
  _descriptorSetLayoutCamera->createCamera();

  _pipeline = std::make_shared<Pipeline>(shader, device);
  _pipeline->createGraphic2D(
      std::vector{_descriptorSetLayoutCamera, _descriptorSetLayoutGraphic, _lightManager->getDescriptorSetLayout()},
      std::vector{LightPush::getPushConstant()}, Vertex2D::getBindingDescription(),
      Vertex2D::getAttributeDescriptions(), render);
}

std::shared_ptr<Sprite> SpriteManager::createSprite(std::shared_ptr<Texture> texture,
                                                    std::shared_ptr<Texture> normalMap) {
  if ((_spritesCreated * _settings->getMaxFramesInFlight()) >= _descriptorPoolSize * _descriptorPool.size()) {
    _descriptorPool.push_back(std::make_shared<DescriptorPool>(_descriptorPoolSize, _device));
  }
  _spritesCreated++;
  return std::make_shared<Sprite>(texture, normalMap, _descriptorSetLayoutCamera, _descriptorSetLayoutGraphic,
                                  _pipeline, _descriptorPool.back(), _commandPool, _commandBuffer, _queue, _device,
                                  _settings);
}

void SpriteManager::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

void SpriteManager::registerSprite(std::shared_ptr<Sprite> sprite) { _sprites.push_back(sprite); }

void SpriteManager::unregisterSprite(std::shared_ptr<Sprite> sprite) {
  _sprites.erase(std::remove(_sprites.begin(), _sprites.end(), sprite), _sprites.end());
}

void SpriteManager::draw(int currentFrame) {
  vkCmdBindPipeline(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline->getPipeline());

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = std::get<1>(_settings->getResolution());
  viewport.width = std::get<0>(_settings->getResolution());
  viewport.height = -std::get<1>(_settings->getResolution());
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(_commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(std::get<0>(_settings->getResolution()), std::get<1>(_settings->getResolution()));
  vkCmdSetScissor(_commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  LightPush pushConstants;
  pushConstants.cameraPosition = _camera->getViewParameters()->eye;
  vkCmdPushConstants(_commandBuffer->getCommandBuffer()[currentFrame], _pipeline->getPipelineLayout(),
                     VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LightPush), &pushConstants);

  vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                          _pipeline->getPipelineLayout(), 2, 1,
                          &_lightManager->getDescriptorSet()->getDescriptorSets()[currentFrame], 0, nullptr);

  for (auto sprite : _sprites) {
    sprite->setCamera(_camera);
    sprite->draw(currentFrame);
  }
}