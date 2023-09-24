#include "ModelManager.h"
#undef far

Model3DManager::Model3DManager(std::vector<VkFormat> renderFormat,
                               std::shared_ptr<LightManager> lightManager,
                               std::shared_ptr<CommandBuffer> commandBufferTransfer,
                               std::shared_ptr<State> state) {
  _commandBufferTransfer = commandBufferTransfer;
  _lightManager = lightManager;
  _state = state;
  _defaultMaterial = std::make_shared<MaterialPhong>(commandBufferTransfer, state);
  _mesh = std::make_shared<Mesh3D>(state);
  _defaultAnimation = std::make_shared<Animation>(std::vector<std::shared_ptr<NodeGLTF>>{},
                                                  std::vector<std::shared_ptr<SkinGLTF>>{},
                                                  std::vector<std::shared_ptr<AnimationGLTF>>{}, state);

  auto cameraSetLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
  cameraSetLayout->createUniformBuffer();
  _descriptorSetLayout.push_back({"camera", cameraSetLayout});
  _descriptorSetLayout.push_back({"joint", _defaultAnimation->getDescriptorSetLayoutJoints()});
  _descriptorSetLayout.push_back({"texture", _defaultMaterial->getDescriptorSetLayoutTextures()});
  _descriptorSetLayout.push_back({"alphaMask", _defaultMaterial->getDescriptorSetLayoutAlphaCutoff()});
  _descriptorSetLayout.push_back({"light", _lightManager->getDSLLight()});
  _descriptorSetLayout.push_back({"lightVP", _lightManager->getDSLViewProjection(VK_SHADER_STAGE_VERTEX_BIT)});
  _descriptorSetLayout.push_back({"shadowTexture", _lightManager->getDSLShadowTexture()});
  _descriptorSetLayout.push_back({"phongCoefficients", _defaultMaterial->getDescriptorSetLayoutCoefficients()});

  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("../shaders/phong3D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/phong3D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[ModelRenderMode::FULL] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["fragment"] = LightPush::getPushConstant();

    _pipeline[ModelRenderMode::FULL]->createGraphic3D(
        renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        _descriptorSetLayout, defaultPushConstants, _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());

    _pipelineCullOff[ModelRenderMode::FULL] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    _pipelineCullOff[ModelRenderMode::FULL]->createGraphic3D(
        renderFormat, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        _descriptorSetLayout, defaultPushConstants, _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("../shaders/depth3D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    _pipeline[ModelRenderMode::DIRECTIONAL] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipeline[ModelRenderMode::DIRECTIONAL]->createGraphic3DShadow(
        VK_CULL_MODE_NONE, {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT)},
        {_descriptorSetLayout[0], _descriptorSetLayout[1]}, {}, _mesh->getBindingDescription(),
        _mesh->getAttributeDescriptions());
  }
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("../shaders/depth3D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/depth3D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipeline[ModelRenderMode::POINT] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["fragment"] = DepthConstants::getPushConstant(0);
    _pipeline[ModelRenderMode::POINT]->createGraphic3DShadow(VK_CULL_MODE_NONE,
                                                             {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                              shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                             {_descriptorSetLayout[0], _descriptorSetLayout[1]},
                                                             defaultPushConstants, _mesh->getBindingDescription(),
                                                             _mesh->getAttributeDescriptions());
  }
}

std::shared_ptr<Model3D> Model3DManager::createModel3D(const std::vector<std::shared_ptr<NodeGLTF>>& nodes,
                                                       const std::vector<std::shared_ptr<Mesh3D>>& meshes) {
  auto model = std::make_shared<Model3D>(nodes, meshes, _descriptorSetLayout, _commandBufferTransfer, _state);
  model->setAnimation(_defaultAnimation);
  return model;
}
void Model3DManager::registerModel3D(std::shared_ptr<Model3D> model) { _modelsGLTF.push_back(model); }

void Model3DManager::unregisterModel3D(std::shared_ptr<Model3D> model) {
  _modelsGLTF.erase(std::remove(_modelsGLTF.begin(), _modelsGLTF.end(), model), _modelsGLTF.end());
}

void Model3DManager::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

void Model3DManager::draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer) {
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline[ModelRenderMode::FULL]->getPipeline());

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
      pushConstants.far = _lightManager->getPointLights()[lightIndex]->getFar();
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame],
                         _pipeline[ModelRenderMode::POINT]->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(DepthConstants), &pushConstants);
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
      lightIndexTotal += _state->getSettings()->getMaxDirectionalLights();
      view = _lightManager->getPointLights()[lightIndex]->getViewMatrix(face);
      projection = _lightManager->getPointLights()[lightIndex]->getProjectionMatrix();
      model->drawShadow(currentFrame, commandBuffer, _pipeline[ModelRenderMode::POINT],
                        _pipeline[ModelRenderMode::POINT], lightIndexTotal, view, projection, face);
    }
  }
}