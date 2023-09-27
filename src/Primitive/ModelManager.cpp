#include "ModelManager.h"
#undef far

Model3DManager::Model3DManager(std::vector<VkFormat> renderFormat,
                               std::shared_ptr<LightManager> lightManager,
                               std::shared_ptr<CommandBuffer> commandBufferTransfer,
                               std::shared_ptr<State> state) {
  _commandBufferTransfer = commandBufferTransfer;
  _lightManager = lightManager;
  _state = state;
  _defaultMaterialPhong = std::make_shared<MaterialPhong>(commandBufferTransfer, state);
  _defaultMaterialPBR = std::make_shared<MaterialPBR>(commandBufferTransfer, state);
  _mesh = std::make_shared<Mesh3D>(state);
  _defaultAnimation = std::make_shared<Animation>(std::vector<std::shared_ptr<NodeGLTF>>{},
                                                  std::vector<std::shared_ptr<SkinGLTF>>{},
                                                  std::vector<std::shared_ptr<AnimationGLTF>>{}, state);

  _cameraSetLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
  _cameraSetLayout->createUniformBuffer();
  _descriptorSetLayout[MaterialType::PHONG].push_back({"camera", _cameraSetLayout});
  _descriptorSetLayout[MaterialType::PHONG].push_back({"joint", _defaultAnimation->getDescriptorSetLayoutJoints()});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"texture", _defaultMaterialPhong->getDescriptorSetLayoutTextures()});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"alphaMask", _defaultMaterialPhong->getDescriptorSetLayoutAlphaCutoff()});
  _descriptorSetLayout[MaterialType::PHONG].push_back({"light", _lightManager->getDSLLight()});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"lightVP", _lightManager->getDSLViewProjection(VK_SHADER_STAGE_VERTEX_BIT)});
  _descriptorSetLayout[MaterialType::PHONG].push_back({"shadowTexture", _lightManager->getDSLShadowTexture()});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"phongCoefficients", _defaultMaterialPhong->getDescriptorSetLayoutCoefficients()});

  _descriptorSetLayout[MaterialType::PBR].push_back({"camera", _cameraSetLayout});
  _descriptorSetLayout[MaterialType::PBR].push_back({"joint", _defaultAnimation->getDescriptorSetLayoutJoints()});
  _descriptorSetLayout[MaterialType::PBR].push_back({"texture", _defaultMaterialPBR->getDescriptorSetLayoutTextures()});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {"alphaMask", _defaultMaterialPBR->getDescriptorSetLayoutAlphaCutoff()});
  _descriptorSetLayout[MaterialType::PBR].push_back({"light", _lightManager->getDSLLight()});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {"lightVP", _lightManager->getDSLViewProjection(VK_SHADER_STAGE_VERTEX_BIT)});
  _descriptorSetLayout[MaterialType::PBR].push_back({"shadowTexture", _lightManager->getDSLShadowTexture()});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {"phongCoefficients", _defaultMaterialPBR->getDescriptorSetLayoutCoefficients()});

  // initialize Phong
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("../shaders/phong3D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/phong3D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[MaterialType::PHONG] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["fragment"] = LightPush::getPushConstant();

    _pipeline[MaterialType::PHONG]->createGraphic3D(renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
                                                    {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                     shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                    _descriptorSetLayout[MaterialType::PHONG], defaultPushConstants,
                                                    _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());

    _pipelineCullOff[MaterialType::PHONG] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    _pipelineCullOff[MaterialType::PHONG]->createGraphic3D(renderFormat, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
                                                           {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                            shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                           _descriptorSetLayout[MaterialType::PHONG],
                                                           defaultPushConstants, _mesh->getBindingDescription(),
                                                           _mesh->getAttributeDescriptions());
  }
  // initialize PBR
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("../shaders/pbr3D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/pbr3D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[MaterialType::PBR] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["fragment"] = LightPush::getPushConstant();

    _pipeline[MaterialType::PBR]->createGraphic3D(renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
                                                  {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                   shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                  _descriptorSetLayout[MaterialType::PBR], defaultPushConstants,
                                                  _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());

    _pipelineCullOff[MaterialType::PBR] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    _pipelineCullOff[MaterialType::PBR]->createGraphic3D(renderFormat, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
                                                         {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                          shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                         _descriptorSetLayout[MaterialType::PBR], defaultPushConstants,
                                                         _mesh->getBindingDescription(),
                                                         _mesh->getAttributeDescriptions());
  }
  //
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("../shaders/depth3D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    _pipelineDirectional = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineDirectional->createGraphic3DShadow(
        VK_CULL_MODE_NONE, {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT)},
        {{"camera", _cameraSetLayout}, {"joint", _defaultAnimation->getDescriptorSetLayoutJoints()}}, {},
        _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("../shaders/depth3D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/depth3D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelinePoint = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["fragment"] = DepthConstants::getPushConstant(0);
    _pipelinePoint->createGraphic3DShadow(
        VK_CULL_MODE_NONE,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        {{"camera", _cameraSetLayout}, {"joint", _defaultAnimation->getDescriptorSetLayoutJoints()}},
        defaultPushConstants, _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }
}

std::shared_ptr<Model3D> Model3DManager::createModel3D(const std::vector<std::shared_ptr<NodeGLTF>>& nodes,
                                                       const std::vector<std::shared_ptr<Mesh3D>>& meshes) {
  auto model = std::make_shared<Model3D>(nodes, meshes, _cameraSetLayout, _commandBufferTransfer, _state);
  model->setAnimation(_defaultAnimation);
  return model;
}
void Model3DManager::registerModel3D(std::shared_ptr<Model3D> model) { _modelsGLTF.push_back(model); }

void Model3DManager::unregisterModel3D(std::shared_ptr<Model3D> model) {
  _modelsGLTF.erase(std::remove(_modelsGLTF.begin(), _modelsGLTF.end(), model), _modelsGLTF.end());
}

void Model3DManager::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

// todo: choose appropriate pipeline using material from sprite
void Model3DManager::draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer) {
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

  // draw phong
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline[MaterialType::PHONG]->getPipeline());

  auto pipelineLayout = _pipeline[MaterialType::PHONG]->getDescriptorSetLayout();
  auto lightVPLayout = std::find_if(
      pipelineLayout.begin(), pipelineLayout.end(),
      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "lightVP"; });
  if (lightVPLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
        _pipeline[MaterialType::PHONG]->getPipelineLayout(), 5, 1,
        &_lightManager->getDSViewProjection(VK_SHADER_STAGE_VERTEX_BIT)->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  auto shadowTextureLayout = std::find_if(
      pipelineLayout.begin(), pipelineLayout.end(),
      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "shadowTexture"; });
  if (shadowTextureLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline[MaterialType::PHONG]->getPipelineLayout(), 6, 1,
                            &_lightManager->getDSShadowTexture()[currentFrame]->getDescriptorSets()[currentFrame], 0,
                            nullptr);
  }

  auto lightLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                  [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                    return info.first == std::string("light");
                                  });
  if (lightLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline[MaterialType::PHONG]->getPipelineLayout(), 4, 1,
                            &_lightManager->getDSLight()->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  for (auto model : _modelsGLTF) {
    if (model->getMaterialType() == MaterialType::PHONG) {
      model->setCamera(_camera);
      model->draw(currentFrame, commandBuffer, _pipeline[MaterialType::PHONG], _pipelineCullOff[MaterialType::PHONG]);
    }
  }

  // draw PBR
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline[MaterialType::PBR]->getPipeline());
  if (lightVPLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
        _pipeline[MaterialType::PBR]->getPipelineLayout(), 5, 1,
        &_lightManager->getDSViewProjection(VK_SHADER_STAGE_VERTEX_BIT)->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  if (shadowTextureLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline[MaterialType::PBR]->getPipelineLayout(), 6, 1,
                            &_lightManager->getDSShadowTexture()[currentFrame]->getDescriptorSets()[currentFrame], 0,
                            nullptr);
  }

  if (lightLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline[MaterialType::PBR]->getPipelineLayout(), 4, 1,
                            &_lightManager->getDSLight()->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  for (auto model : _modelsGLTF) {
    if (model->getMaterialType() == MaterialType::PBR) {
      model->setCamera(_camera);
      model->draw(currentFrame, commandBuffer, _pipeline[MaterialType::PBR], _pipelineCullOff[MaterialType::PBR]);
    }
  }
}

void Model3DManager::drawShadow(int currentFrame,
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

  if (lightType == LightType::POINT) {
    if (pipeline->getPushConstants().find("fragment") != pipeline->getPushConstants().end()) {
      DepthConstants pushConstants;
      pushConstants.lightPosition = _lightManager->getPointLights()[lightIndex]->getPosition();
      // light camera
      pushConstants.far = _lightManager->getPointLights()[lightIndex]->getFar();
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DepthConstants), &pushConstants);
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
      model->drawShadow(currentFrame, commandBuffer, pipeline, pipeline, lightIndexTotal, view, projection, face);
    }
    if (lightType == LightType::POINT) {
      lightIndexTotal += _state->getSettings()->getMaxDirectionalLights();
      view = _lightManager->getPointLights()[lightIndex]->getViewMatrix(face);
      projection = _lightManager->getPointLights()[lightIndex]->getProjectionMatrix();
      model->drawShadow(currentFrame, commandBuffer, pipeline, pipeline, lightIndexTotal, view, projection, face);
    }
  }
}