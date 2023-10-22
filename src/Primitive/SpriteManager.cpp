#include "SpriteManager.h"
#include <ranges>

SpriteManager::SpriteManager(std::vector<VkFormat> renderFormat,
                             std::shared_ptr<LightManager> lightManager,
                             std::shared_ptr<CommandBuffer> commandBufferTransfer,
                             std::shared_ptr<State> state) {
  _commandBufferTransfer = commandBufferTransfer;
  _state = state;
  _lightManager = lightManager;
  _defaultMaterialPhong = std::make_shared<MaterialPhong>(commandBufferTransfer, state);
  _defaultMaterialPBR = std::make_shared<MaterialPBR>(commandBufferTransfer, state);

  _defaultMesh = std::make_shared<Mesh2D>(state);
  // 3   0
  // 2   1
  _defaultMesh->setVertices(
      {Vertex2D{{0.5f, 0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.f, 0.f}},
       Vertex2D{{0.5f, -0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.f, 0.f}},
       Vertex2D{{-0.5f, -0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.f, 0.f}},
       Vertex2D{{-0.5f, 0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.f, 0.f}}},
      commandBufferTransfer);
  _defaultMesh->setIndexes({0, 3, 2, 2, 1, 0}, commandBufferTransfer);

  auto cameraSetLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
  cameraSetLayout->createUniformBuffer();
  // layout for PHONG
  _descriptorSetLayout[MaterialType::PHONG].push_back({"camera", cameraSetLayout});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"lightVP", _lightManager->getDSLViewProjection(VK_SHADER_STAGE_VERTEX_BIT)});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"texture", _defaultMaterialPhong->getDescriptorSetLayoutTextures()});
  _descriptorSetLayout[MaterialType::PHONG].push_back({"light", _lightManager->getDSLLight()});
  _descriptorSetLayout[MaterialType::PHONG].push_back({"shadowTexture", _lightManager->getDSLShadowTexture()});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"materialCoefficients", _defaultMaterialPhong->getDescriptorSetLayoutCoefficients()});

  // layout for PBR
  _descriptorSetLayout[MaterialType::PBR].push_back({"camera", cameraSetLayout});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {"lightVP", _lightManager->getDSLViewProjection(VK_SHADER_STAGE_VERTEX_BIT)});
  _descriptorSetLayout[MaterialType::PBR].push_back({"texture", _defaultMaterialPBR->getDescriptorSetLayoutTextures()});
  _descriptorSetLayout[MaterialType::PBR].push_back({"light", _lightManager->getDSLLight()});
  _descriptorSetLayout[MaterialType::PBR].push_back({"shadowTexture", _lightManager->getDSLShadowTexture()});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {"materialCoefficients", _defaultMaterialPBR->getDescriptorSetLayoutCoefficients()});

  // layout for Normal Phong
  _descriptorSetLayoutNormal[MaterialType::PHONG].push_back({"camera", cameraSetLayout});
  _descriptorSetLayoutNormal[MaterialType::PHONG].push_back(
      {"lightVP", _lightManager->getDSLViewProjection(VK_SHADER_STAGE_VERTEX_BIT)});
  _descriptorSetLayoutNormal[MaterialType::PHONG].push_back(
      {"texture", _defaultMaterialPhong->getDescriptorSetLayoutTextures()});
  // layout for Normal PBR
  _descriptorSetLayoutNormal[MaterialType::PBR].push_back({"camera", cameraSetLayout});
  _descriptorSetLayoutNormal[MaterialType::PBR].push_back(
      {"lightVP", _lightManager->getDSLViewProjection(VK_SHADER_STAGE_VERTEX_BIT)});
  _descriptorSetLayoutNormal[MaterialType::PBR].push_back(
      {"texture", _defaultMaterialPBR->getDescriptorSetLayoutTextures()});
  // initialize Phong
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("../shaders/phong2D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/phong2D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[MaterialType::PHONG] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipeline[MaterialType::PHONG]->createGraphic2D(
        renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        _descriptorSetLayout[MaterialType::PHONG],
        std::map<std::string, VkPushConstantRange>{{std::string("fragment"), LightPush::getPushConstant()}},
        _defaultMesh->getBindingDescription(), _defaultMesh->getAttributeDescriptions());
    // wireframe one
    _pipelineWireframe[MaterialType::PHONG] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineWireframe[MaterialType::PHONG]->createGraphic2D(
        renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        _descriptorSetLayout[MaterialType::PHONG],
        std::map<std::string, VkPushConstantRange>{{std::string("fragment"), LightPush::getPushConstant()}},
        _defaultMesh->getBindingDescription(), _defaultMesh->getAttributeDescriptions());
  }

  // initialize PBR
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("../shaders/pbr2D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/pbr2D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[MaterialType::PBR] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipeline[MaterialType::PBR]->createGraphic2D(
        renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        _descriptorSetLayout[MaterialType::PBR],
        std::map<std::string, VkPushConstantRange>{{std::string("fragment"), LightPush::getPushConstant()}},
        _defaultMesh->getBindingDescription(), _defaultMesh->getAttributeDescriptions());
    // wireframe one
    _pipelineWireframe[MaterialType::PBR] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineWireframe[MaterialType::PBR]->createGraphic2D(
        renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        _descriptorSetLayout[MaterialType::PBR],
        std::map<std::string, VkPushConstantRange>{{std::string("fragment"), LightPush::getPushConstant()}},
        _defaultMesh->getBindingDescription(), _defaultMesh->getAttributeDescriptions());
  }
  // initialize Normal for Phong
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("../shaders/phong2D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/debugPhong2D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipelineNormal[MaterialType::PHONG] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineNormal[MaterialType::PHONG]->createGraphic2D(renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
                                                          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                          _descriptorSetLayoutNormal[MaterialType::PHONG], {},
                                                          _defaultMesh->getBindingDescription(),
                                                          _defaultMesh->getAttributeDescriptions());
    // wireframe one
    _pipelineNormalWireframe[MaterialType::PHONG] = std::make_shared<Pipeline>(_state->getSettings(),
                                                                               _state->getDevice());
    _pipelineNormalWireframe[MaterialType::PHONG]->createGraphic2D(
        renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        _descriptorSetLayoutNormal[MaterialType::PHONG], {}, _defaultMesh->getBindingDescription(),
        _defaultMesh->getAttributeDescriptions());
  }
  // initialize Normal for PBR
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("../shaders/pbr2D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/debugPBR2D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipelineNormal[MaterialType::PBR] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineNormal[MaterialType::PBR]->createGraphic2D(renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
                                                        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                        _descriptorSetLayoutNormal[MaterialType::PBR], {},
                                                        _defaultMesh->getBindingDescription(),
                                                        _defaultMesh->getAttributeDescriptions());
    // wireframe one
    _pipelineNormalWireframe[MaterialType::PBR] = std::make_shared<Pipeline>(_state->getSettings(),
                                                                             _state->getDevice());
    _pipelineNormalWireframe[MaterialType::PBR]->createGraphic2D(
        renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        _descriptorSetLayoutNormal[MaterialType::PBR], {}, _defaultMesh->getBindingDescription(),
        _defaultMesh->getAttributeDescriptions());
  }
  // initialize depth directional
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("../shaders/depth2D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    _pipelineDirectional = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineDirectional->createGraphic2DShadow(
        VK_CULL_MODE_NONE, {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT)}, {{"camera", cameraSetLayout}}, {},
        _defaultMesh->getBindingDescription(), _defaultMesh->getAttributeDescriptions());
  }

  // initialize depth point
  {
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["fragment"] = DepthConstants::getPushConstant(0);

    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("../shaders/depth2D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/depth2D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelinePoint = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelinePoint->createGraphic2DShadow(VK_CULL_MODE_NONE,
                                          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                          {{"camera", cameraSetLayout}}, defaultPushConstants,
                                          _defaultMesh->getBindingDescription(),
                                          _defaultMesh->getAttributeDescriptions());
  }
}

std::shared_ptr<Sprite> SpriteManager::createSprite() {
  _spritesCreated++;
  auto sprite = std::make_shared<Sprite>(_defaultMesh, _commandBufferTransfer, _state);
  return sprite;
}

void SpriteManager::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

void SpriteManager::registerSprite(std::shared_ptr<Sprite> sprite) { _sprites.push_back(sprite); }

void SpriteManager::unregisterSprite(std::shared_ptr<Sprite> sprite) {
  _sprites.erase(std::remove(_sprites.begin(), _sprites.end(), sprite), _sprites.end());
}

void SpriteManager::draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer, DrawType drawType) {
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

  // define draw spirte lambda
  auto drawSprite = [&](MaterialType materialType, DrawType drawType) {
    auto pipeline = _pipeline[materialType];
    if ((drawType & DrawType::WIREFRAME) == DrawType::WIREFRAME) pipeline = _pipelineWireframe[materialType];

    if ((drawType & DrawType::NORMAL) == DrawType::NORMAL) {
      pipeline = _pipelineNormal[materialType];
      if ((drawType & DrawType::WIREFRAME) == DrawType::WIREFRAME) pipeline = _pipelineNormalWireframe[materialType];
    }

    auto pipelineLayout = pipeline->getDescriptorSetLayout();

    vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline->getPipeline());

    auto lightVPLayout = std::find_if(
        pipelineLayout.begin(), pipelineLayout.end(),
        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "lightVP"; });
    if (lightVPLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(
          commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
          pipeline->getPipelineLayout(), 1, 1,
          &_lightManager->getDSViewProjection(VK_SHADER_STAGE_VERTEX_BIT)->getDescriptorSets()[currentFrame], 0,
          nullptr);
    }

    auto lightLayout = std::find_if(
        pipelineLayout.begin(), pipelineLayout.end(),
        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "light"; });
    if (lightLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 3, 1,
                              &_lightManager->getDSLight()->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    auto shadowTextureLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                            [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                              return info.first == "shadowTexture";
                                            });
    if (shadowTextureLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 4, 1,
                              &_lightManager->getDSShadowTexture()[currentFrame]->getDescriptorSets()[currentFrame], 0,
                              nullptr);
    }

    for (auto sprite : _sprites) {
      if (sprite->getMaterialType() == materialType) {
        sprite->setCamera(_camera);
        sprite->draw(currentFrame, commandBuffer, pipeline);
      }
    }
  };

  drawSprite(MaterialType::PHONG, drawType);
  drawSprite(MaterialType::PBR, drawType);
}

void SpriteManager::drawShadow(int currentFrame,
                               std::shared_ptr<CommandBuffer> commandBuffer,
                               LightType lightType,
                               int lightIndex,
                               int face) {
  auto pipeline = _pipelineDirectional;
  if (lightType == LightType::POINT) pipeline = _pipelinePoint;

  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->getPipeline());
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

  if (pipeline->getPushConstants().find("fragment") != pipeline->getPushConstants().end()) {
    if (lightType == LightType::POINT) {
      DepthConstants pushConstants;
      pushConstants.lightPosition = _lightManager->getPointLights()[lightIndex]->getPosition();
      // light camera
      pushConstants.far = _lightManager->getPointLights()[lightIndex]->getFar();
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DepthConstants), &pushConstants);
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
      sprite->drawShadow(currentFrame, commandBuffer, pipeline, lightIndexTotal, view, projection, face);
    }
    if (lightType == LightType::POINT) {
      lightIndexTotal += _state->getSettings()->getMaxDirectionalLights();
      view = _lightManager->getPointLights()[lightIndex]->getViewMatrix(face);
      projection = _lightManager->getPointLights()[lightIndex]->getProjectionMatrix();
      sprite->drawShadow(currentFrame, commandBuffer, pipeline, lightIndexTotal, view, projection, face);
    }
  }
}