#include "SpriteManager.h"
#include <ranges>

SpriteManager::SpriteManager(std::shared_ptr<LightManager> lightManager,
                             std::shared_ptr<CommandPool> commandPool,
                             std::shared_ptr<CommandBuffer> commandBuffer,
                             std::shared_ptr<Queue> queue,
                             std::shared_ptr<DescriptorPool> descriptorPool,
                             std::shared_ptr<RenderPass> render,
                             std::shared_ptr<RenderPass> renderDepth,
                             std::shared_ptr<Device> device,
                             std::shared_ptr<Settings> settings) {
  _commandPool = commandPool;
  _commandBuffer = commandBuffer;
  _queue = queue;
  _device = device;
  _settings = settings;
  _lightManager = lightManager;
  _descriptorPool = descriptorPool;

  {
    auto setLayout = std::make_shared<DescriptorSetLayout>(device);
    setLayout->createCamera();
    _descriptorSetLayout.push_back(setLayout);
  }
  {
    auto setLayout = std::make_shared<DescriptorSetLayout>(device);
    setLayout->createGraphicModel();
    _descriptorSetLayout.push_back(setLayout);
  }
  { _descriptorSetLayout.push_back(_lightManager->getDescriptorSetLayout()); }

  {
    auto shader = std::make_shared<Shader>(device);
    shader->add("../shaders/phong2D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/phong2D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[SpriteRenderMode::FULL] = std::make_shared<Pipeline>(shader, device);
    _pipeline[SpriteRenderMode::FULL]->createGraphic2D(
        VK_CULL_MODE_BACK_BIT, _descriptorSetLayout,
        std::map<std::string, VkPushConstantRange>{{std::string("fragment"), LightPush::getPushConstant()}},
        Vertex2D::getBindingDescription(), Vertex2D::getAttributeDescriptions(), render);
  }
  {
    auto shader = std::make_shared<Shader>(device);
    shader->add("../shaders/depth2D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/depth2D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipeline[SpriteRenderMode::DEPTH] = std::make_shared<Pipeline>(shader, device);
    _pipeline[SpriteRenderMode::DEPTH]->createGraphic2D(VK_CULL_MODE_BACK_BIT, {_descriptorSetLayout[0]}, {},
                                                        Vertex2D::getBindingDescription(),
                                                        Vertex2D::getAttributeDescriptions(), renderDepth);
  }
}

std::shared_ptr<Sprite> SpriteManager::createSprite(std::shared_ptr<Texture> texture,
                                                    std::shared_ptr<Texture> normalMap) {
  _spritesCreated++;
  return std::make_shared<Sprite>(texture, normalMap, _descriptorSetLayout, _descriptorPool, _commandPool,
                                  _commandBuffer, _queue, _device, _settings);
}

void SpriteManager::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

void SpriteManager::registerSprite(std::shared_ptr<Sprite> sprite) { _sprites.push_back(sprite); }

void SpriteManager::unregisterSprite(std::shared_ptr<Sprite> sprite) {
  _sprites.erase(std::remove(_sprites.begin(), _sprites.end(), sprite), _sprites.end());
}

void SpriteManager::draw(SpriteRenderMode mode, int currentFrame) {
  vkCmdBindPipeline(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline[mode]->getPipeline());

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

  if (_pipeline[mode]->getPushConstants().find("fragment") != _pipeline[mode]->getPushConstants().end()) {
    LightPush pushConstants;
    pushConstants.cameraPosition = _camera->getViewParameters()->eye;
    vkCmdPushConstants(_commandBuffer->getCommandBuffer()[currentFrame], _pipeline[mode]->getPipelineLayout(),
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LightPush), &pushConstants);
  }

  if (_pipeline[mode]->getDescriptorSetLayout().size() > 2) {
    vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline[mode]->getPipelineLayout(), 2, 1,
                            &_lightManager->getDescriptorSet()->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  for (auto sprite : _sprites) {
    sprite->setCamera(_camera);
    sprite->draw(currentFrame, _pipeline[mode]);
  }
}