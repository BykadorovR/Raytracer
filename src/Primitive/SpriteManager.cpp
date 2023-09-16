#include "SpriteManager.h"
#include <ranges>

SpriteManager::SpriteManager(std::vector<VkFormat> renderFormat,
                             std::shared_ptr<LightManager> lightManager,
                             std::shared_ptr<CommandBuffer> commandBufferTransfer,
                             std::shared_ptr<State> state) {
  _commandBufferTransfer = commandBufferTransfer;
  _state = state;
  _lightManager = lightManager;
  _defaultMaterial = std::make_shared<MaterialSpritePhong>(commandBufferTransfer, state);

  auto cameraSetLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
  cameraSetLayout->createUniformBuffer();
  _descriptorSetLayout.push_back({"camera", cameraSetLayout});
  _descriptorSetLayout.push_back({"texture", _defaultMaterial->getDescriptorSetLayoutTextures()});
  _descriptorSetLayout.push_back({"light", _lightManager->getDSLLight()});
  _descriptorSetLayout.push_back({"lightVP", _lightManager->getDSLViewProjection(VK_SHADER_STAGE_VERTEX_BIT)});
  _descriptorSetLayout.push_back({"shadowTexture", _lightManager->getDSLShadowTexture()});
  _descriptorSetLayout.push_back({"phongCoefficients", _defaultMaterial->getDescriptorSetLayoutCoefficients()});

  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("../shaders/phong2D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/phong2D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[SpriteRenderMode::FULL] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipeline[SpriteRenderMode::FULL]->createGraphic2D(
        renderFormat, VK_CULL_MODE_BACK_BIT,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        _descriptorSetLayout,
        std::map<std::string, VkPushConstantRange>{{std::string("fragment"), LightPush::getPushConstant()}},
        Vertex2D::getBindingDescription(), Vertex2D::getAttributeDescriptions());
  }
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("../shaders/depth2D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    _pipeline[SpriteRenderMode::DIRECTIONAL] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipeline[SpriteRenderMode::DIRECTIONAL]->createGraphic2DShadow(
        VK_CULL_MODE_NONE, {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT)}, {_descriptorSetLayout[0]}, {},
        Vertex2D::getBindingDescription(), Vertex2D::getAttributeDescriptions());
  }
  {
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["fragment"] = DepthConstants::getPushConstant(0);

    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("../shaders/depth2D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/depth2D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipeline[SpriteRenderMode::POINT] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipeline[SpriteRenderMode::POINT]->createGraphic2DShadow(
        VK_CULL_MODE_NONE,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        {_descriptorSetLayout[0]}, defaultPushConstants, Vertex2D::getBindingDescription(),
        Vertex2D::getAttributeDescriptions());
  }
}

std::shared_ptr<Sprite> SpriteManager::createSprite() {
  _spritesCreated++;
  auto sprite = std::make_shared<Sprite>(_descriptorSetLayout, _commandBufferTransfer, _state);
  sprite->setMaterial(_defaultMaterial);
  return sprite;
}

std::shared_ptr<Sprite> SpriteManager::createSprite(std::shared_ptr<MaterialSpritePhong> material) {
  _spritesCreated++;
  auto sprite = std::make_shared<Sprite>(_descriptorSetLayout, _commandBufferTransfer, _state);
  sprite->setMaterial(material);
  return sprite;
}

void SpriteManager::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

void SpriteManager::registerSprite(std::shared_ptr<Sprite> sprite) { _sprites.push_back(sprite); }

void SpriteManager::unregisterSprite(std::shared_ptr<Sprite> sprite) {
  _sprites.erase(std::remove(_sprites.begin(), _sprites.end(), sprite), _sprites.end());
}

void SpriteManager::draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer) {
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline[SpriteRenderMode::FULL]->getPipeline());

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = std::get<1>(_state->getSettings()->getResolution());
  viewport.width = std::get<0>(_state->getSettings()->getResolution());
  viewport.height = -std::get<1>(_state->getSettings()->getResolution());
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(std::get<0>(_state->getSettings()->getResolution()),
                              std::get<1>(_state->getSettings()->getResolution()));
  vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  auto pipelineLayout = _pipeline[SpriteRenderMode::FULL]->getDescriptorSetLayout();
  auto lightLayout = std::find_if(
      pipelineLayout.begin(), pipelineLayout.end(),
      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "light"; });
  if (lightLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline[SpriteRenderMode::FULL]->getPipelineLayout(), 2, 1,
                            &_lightManager->getDSLight()->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  auto lightVPLayout = std::find_if(
      pipelineLayout.begin(), pipelineLayout.end(),
      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "lightVP"; });
  if (lightVPLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
        _pipeline[SpriteRenderMode::FULL]->getPipelineLayout(), 3, 1,
        &_lightManager->getDSViewProjection(VK_SHADER_STAGE_VERTEX_BIT)->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  auto shadowTextureLayout = std::find_if(
      pipelineLayout.begin(), pipelineLayout.end(),
      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "shadowTexture"; });
  if (shadowTextureLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline[SpriteRenderMode::FULL]->getPipelineLayout(), 4, 1,
                            &_lightManager->getDSShadowTexture()[currentFrame]->getDescriptorSets()[currentFrame], 0,
                            nullptr);
  }

  for (auto sprite : _sprites) {
    sprite->setCamera(_camera);
    sprite->draw(currentFrame, commandBuffer, _pipeline[SpriteRenderMode::FULL]);
  }
}

void SpriteManager::drawShadow(int currentFrame,
                               std::shared_ptr<CommandBuffer> commandBuffer,
                               LightType lightType,
                               int lightIndex,
                               int face) {
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline[(SpriteRenderMode)lightType]->getPipeline());
  std::tuple<int, int> resolution;
  if (lightType == LightType::DIRECTIONAL) {
    resolution = _lightManager->getDirectionalLights()[lightIndex]
                     ->getDepthTexture()[currentFrame]
                     ->getImageView()
                     ->getImage()
                     ->getResolution();
  }
  if (lightType == LightType::POINT) {
    resolution = _lightManager->getPointLights()[lightIndex]
                     ->getDepthCubemap()[currentFrame]
                     ->getTexture()
                     ->getImageView()
                     ->getImage()
                     ->getResolution();
  }

  // Cube Maps have been specified to follow the RenderMan specification (for whatever reason),
  // and RenderMan assumes the images' origin being in the upper left so we don't need to swap anything
  VkViewport viewport{};
  if (lightType == LightType::DIRECTIONAL) {
    viewport.x = 0.0f;
    viewport.y = std::get<1>(resolution);
    viewport.width = std::get<0>(resolution);
    viewport.height = -std::get<1>(resolution);
  } else if (lightType == LightType::POINT) {
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = std::get<0>(resolution);
    viewport.height = std::get<1>(resolution);
  }

  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(std::get<0>(resolution), std::get<1>(resolution));
  vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  if (_pipeline[SpriteRenderMode::POINT]->getPushConstants().find("fragment") !=
      _pipeline[SpriteRenderMode::POINT]->getPushConstants().end()) {
    if (lightType == LightType::POINT) {
      DepthConstants pushConstants;
      pushConstants.lightPosition = _lightManager->getPointLights()[lightIndex]->getPosition();
      // light camera
      pushConstants.far = _lightManager->getPointLights()[lightIndex]->getFar();
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame],
                         _pipeline[SpriteRenderMode::POINT]->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(DepthConstants), &pushConstants);
    }
  }

  for (auto sprite : _sprites) {
    if (sprite->isDepthEnabled() == false) continue;
    glm::mat4 view(1.f);
    glm::mat4 projection(1.f);
    int lightIndexTotal = lightIndex;
    if (lightType == LightType::DIRECTIONAL) {
      view = _lightManager->getDirectionalLights()[lightIndex]->getViewMatrix();
      projection = _lightManager->getDirectionalLights()[lightIndex]->getProjectionMatrix();
      sprite->drawShadow(currentFrame, commandBuffer, _pipeline[SpriteRenderMode::DIRECTIONAL], lightIndexTotal, view,
                         projection, face);
    }
    if (lightType == LightType::POINT) {
      lightIndexTotal += _state->getSettings()->getMaxDirectionalLights();
      view = _lightManager->getPointLights()[lightIndex]->getViewMatrix(face);
      projection = _lightManager->getPointLights()[lightIndex]->getProjectionMatrix();
      sprite->drawShadow(currentFrame, commandBuffer, _pipeline[SpriteRenderMode::POINT], lightIndexTotal, view,
                         projection, face);
    }
  }
}