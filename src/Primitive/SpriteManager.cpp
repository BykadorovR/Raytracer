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
    _shadowDescriptorSetLayout = std::make_shared<DescriptorSetLayout>(device);
    _shadowDescriptorSetLayout->createShadow();
    _descriptorSetLayout.push_back(_shadowDescriptorSetLayout);
  }

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

  {
    _uniformShadow = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(glm::mat4), commandPool,
                                                     queue, device);
    _shadowDescriptorSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), _shadowDescriptorSetLayout,
                                                           descriptorPool, device);
    _shadowDescriptorSet->createShadow(_uniformShadow);
  }

  _cameraOrtho = std::make_shared<CameraOrtho>();
}

std::shared_ptr<Sprite> SpriteManager::createSprite(std::shared_ptr<Texture> texture,
                                                    std::shared_ptr<Texture> normalMap,
                                                    std::shared_ptr<Texture> shadowMap) {
  _spritesCreated++;
  return std::make_shared<Sprite>(texture, normalMap, shadowMap, _descriptorSetLayout, _descriptorPool, _commandPool,
                                  _commandBuffer, _queue, _device, _settings);
}

void SpriteManager::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

void SpriteManager::registerSprite(std::shared_ptr<Sprite> sprite) { _sprites.push_back(sprite); }

void SpriteManager::unregisterSprite(std::shared_ptr<Sprite> sprite) {
  _sprites.erase(std::remove(_sprites.begin(), _sprites.end(), sprite), _sprites.end());
}

void SpriteManager::draw(int currentFrame, SpriteRenderMode mode) {
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
    pushConstants.cameraPosition = _camera->getEye();
    vkCmdPushConstants(_commandBuffer->getCommandBuffer()[currentFrame], _pipeline[mode]->getPipelineLayout(),
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LightPush), &pushConstants);
  }

  auto position = _lightManager->getPointLights()[0]->getPosition();
  _cameraOrtho->setViewParameters(_camera->getEye(), _camera->getDirection(), _camera->getUp());
  //_cameraOrtho->setViewParameters(position, -position, glm::vec3(0.f, 1.f, 0.f));
  _cameraOrtho->setProjectionParameters({-10.f, 10.f, -10.f, 10.f}, 0.1f, 40.f);

  if (mode == SpriteRenderMode::FULL) {
    glm::mat4 shadowCamera = _cameraOrtho->getProjection() * _cameraOrtho->getView();

    void* data;
    vkMapMemory(_device->getLogicalDevice(), _uniformShadow->getBuffer()[currentFrame]->getMemory(), 0,
                sizeof(glm::mat4), 0, &data);
    memcpy(data, &shadowCamera, sizeof(glm::mat4));
    vkUnmapMemory(_device->getLogicalDevice(), _uniformShadow->getBuffer()[currentFrame]->getMemory());

    vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline[mode]->getPipelineLayout(), 3, 1,
                            &_shadowDescriptorSet->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  if (_pipeline[mode]->getDescriptorSetLayout().size() > 2) {
    vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline[mode]->getPipelineLayout(), 2, 1,
                            &_lightManager->getDescriptorSet()->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  for (auto sprite : _sprites) {
    if (mode == SpriteRenderMode::DEPTH) {
      sprite->setCamera(_cameraOrtho);
    }
    if (mode == SpriteRenderMode::FULL) {
      sprite->setCamera(_camera);
    }
    sprite->draw(currentFrame, mode, _pipeline[mode]);
  }
}