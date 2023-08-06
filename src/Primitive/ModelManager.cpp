#include "ModelManager.h"

Model3DManager::Model3DManager(std::shared_ptr<LightManager> lightManager,
                               std::shared_ptr<CommandBuffer> commandBufferTransfer,
                               std::shared_ptr<DescriptorPool> descriptorPool,
                               std::shared_ptr<Device> device,
                               std::shared_ptr<Settings> settings) {
  _lightManager = lightManager;
  _commandBufferTransfer = commandBufferTransfer;
  _device = device;
  _settings = settings;
  _descriptorPool = descriptorPool;
  {
    auto setLayout = std::make_shared<DescriptorSetLayout>(device);
    setLayout->createBuffer();
    _descriptorSetLayout.push_back({"camera", setLayout});
  }
  {
    auto setLayout = std::make_shared<DescriptorSetLayout>(device);
    setLayout->createJoints();
    _descriptorSetLayout.push_back({"joint", setLayout});
  }
  {
    auto setLayout = std::make_shared<DescriptorSetLayout>(device);
    setLayout->createGraphicModel();
    _descriptorSetLayout.push_back({"texture", setLayout});
  }
  {
    auto setLayout = std::make_shared<DescriptorSetLayout>(device);
    setLayout->createModelAuxilary();
    _descriptorSetLayout.push_back({"alphaMask", setLayout});
  }
  { _descriptorSetLayout.push_back({"light", _lightManager->getDSLLight()}); }

  { _descriptorSetLayout.push_back({"lightVP", _lightManager->getDSLViewProjection(VK_SHADER_STAGE_VERTEX_BIT)}); }

  { _descriptorSetLayout.push_back({"shadowTexture", _lightManager->getDSLShadowTexture()}); }

  {
    auto shader = std::make_shared<Shader>(device);
    shader->add("../shaders/phong3D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/phong3D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[ModelRenderMode::FULL] = std::make_shared<Pipeline>(settings, device);
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["vertex"] = PushConstants::getPushConstant(sizeof(LightPush));
    defaultPushConstants["fragment"] = LightPush::getPushConstant();

    _pipeline[ModelRenderMode::FULL]->createGraphic3D(VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
                                                      {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                       shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                      _descriptorSetLayout, defaultPushConstants,
                                                      Vertex3D::getBindingDescription(),
                                                      Vertex3D::getAttributeDescriptions());

    _pipelineCullOff[ModelRenderMode::FULL] = std::make_shared<Pipeline>(settings, device);
    _pipelineCullOff[ModelRenderMode::FULL]->createGraphic3D(VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
                                                             {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                              shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                             _descriptorSetLayout, defaultPushConstants,
                                                             Vertex3D::getBindingDescription(),
                                                             Vertex3D::getAttributeDescriptions());
  }
  {
    auto shader = std::make_shared<Shader>(device);
    shader->add("../shaders/depth3D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    _pipeline[ModelRenderMode::DIRECTIONAL] = std::make_shared<Pipeline>(settings, device);
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["vertex"] = PushConstants::getPushConstant(0);
    _pipeline[ModelRenderMode::DIRECTIONAL]->createGraphic3DShadow(
        VK_CULL_MODE_NONE, {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT)},
        {_descriptorSetLayout[0], _descriptorSetLayout[1]}, defaultPushConstants, Vertex3D::getBindingDescription(),
        Vertex3D::getAttributeDescriptions());
  }
  {
    auto shader = std::make_shared<Shader>(device);
    shader->add("../shaders/depth3D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/depth3D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipeline[ModelRenderMode::POINT] = std::make_shared<Pipeline>(settings, device);
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["vertex"] = PushConstants::getPushConstant(0);
    defaultPushConstants["fragment"] = DepthConstants::getPushConstant(sizeof(PushConstants));
    _pipeline[ModelRenderMode::POINT]->createGraphic3DShadow(VK_CULL_MODE_NONE,
                                                             {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                              shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                             {_descriptorSetLayout[0], _descriptorSetLayout[1]},
                                                             defaultPushConstants, Vertex3D::getBindingDescription(),
                                                             Vertex3D::getAttributeDescriptions());
  }
}

std::shared_ptr<ModelGLTF> Model3DManager::createModelGLTF(std::string path) {
  return std::make_shared<ModelGLTF>(path, _descriptorSetLayout, _lightManager, _descriptorPool, _commandBufferTransfer,
                                     _device, _settings);
}
void Model3DManager::registerModelGLTF(std::shared_ptr<Model> model) { _modelsGLTF.push_back(model); }

void Model3DManager::unregisterModelGLTF(std::shared_ptr<Model> model) {
  _modelsGLTF.erase(std::remove(_modelsGLTF.begin(), _modelsGLTF.end(), model), _modelsGLTF.end());
}

void Model3DManager::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

void Model3DManager::draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer) {
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline[ModelRenderMode::FULL]->getPipeline());

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = std::get<1>(_settings->getResolution());
  viewport.width = std::get<0>(_settings->getResolution());
  viewport.height = -std::get<1>(_settings->getResolution());
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(std::get<0>(_settings->getResolution()), std::get<1>(_settings->getResolution()));
  vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  auto pipelineLayout = _pipeline[ModelRenderMode::FULL]->getDescriptorSetLayout();
  auto lightVPLayout = std::find_if(
      pipelineLayout.begin(), pipelineLayout.end(),
      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "lightVP"; });
  if (lightVPLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
        _pipeline[ModelRenderMode::FULL]->getPipelineLayout(), 5, 1,
        &_lightManager->getDSViewProjection(VK_SHADER_STAGE_VERTEX_BIT)->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  auto shadowTextureLayout = std::find_if(
      pipelineLayout.begin(), pipelineLayout.end(),
      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "shadowTexture"; });
  if (shadowTextureLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline[ModelRenderMode::FULL]->getPipelineLayout(), 6, 1,
                            &_lightManager->getDSShadowTexture()[currentFrame]->getDescriptorSets()[currentFrame], 0,
                            nullptr);
  }

  auto lightLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                  [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                    return info.first == std::string("light");
                                  });
  if (lightLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline[ModelRenderMode::FULL]->getPipelineLayout(), 4, 1,
                            &_lightManager->getDSLight()->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  for (auto model : _modelsGLTF) {
    model->setCamera(_camera);
    model->draw(currentFrame, commandBuffer, _pipeline[ModelRenderMode::FULL], _pipelineCullOff[ModelRenderMode::FULL]);
  }
}

void Model3DManager::updateAnimation(float deltaTime) {
  for (auto model : _modelsGLTF) {
    if (model) model->updateAnimation(deltaTime);
  }
}

void Model3DManager::drawShadow(int currentFrame,
                                std::shared_ptr<CommandBuffer> commandBuffer,
                                LightType lightType,
                                int lightIndex,
                                int face) {
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline[(ModelRenderMode)lightType]->getPipeline());

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

  if (_pipeline[ModelRenderMode::POINT]->getPushConstants().find("fragment") !=
      _pipeline[ModelRenderMode::POINT]->getPushConstants().end()) {
    if (lightType == LightType::POINT) {
      DepthConstants pushConstants;
      pushConstants.lightPosition = _lightManager->getPointLights()[lightIndex]->getPosition();
      // light camera
      pushConstants.far = 100.f;
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame],
                         _pipeline[ModelRenderMode::POINT]->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT,
                         sizeof(PushConstants), sizeof(DepthConstants), &pushConstants);
    }
  }

  for (auto model : _modelsGLTF) {
    if (model->isDepthEnabled() == false) continue;
    glm::mat4 view(1.f);
    glm::mat4 projection(1.f);
    int lightIndexTotal = lightIndex;
    if (lightType == LightType::DIRECTIONAL) {
      view = _lightManager->getDirectionalLights()[lightIndex]->getViewMatrix();
      projection = _lightManager->getDirectionalLights()[lightIndex]->getProjectionMatrix();
      model->drawShadow(currentFrame, commandBuffer, _pipeline[ModelRenderMode::DIRECTIONAL],
                        _pipeline[ModelRenderMode::DIRECTIONAL], lightIndexTotal, view, projection, face);
    }
    if (lightType == LightType::POINT) {
      lightIndexTotal += _settings->getMaxDirectionalLights();
      view = _lightManager->getPointLights()[lightIndex]->getViewMatrix(face);
      projection = _lightManager->getPointLights()[lightIndex]->getProjectionMatrix();
      model->drawShadow(currentFrame, commandBuffer, _pipeline[ModelRenderMode::POINT],
                        _pipeline[ModelRenderMode::POINT], lightIndexTotal, view, projection, face);
    }
  }
}