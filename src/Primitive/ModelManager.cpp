#include "ModelManager.h"
#undef far

Model3DManager::Model3DManager(std::vector<VkFormat> renderFormat,
                               std::shared_ptr<LightManager> lightManager,
                               std::shared_ptr<CommandBuffer> commandBufferTransfer,
                               std::shared_ptr<ResourceManager> resourceManager,
                               std::shared_ptr<State> state) {
  _commandBufferTransfer = commandBufferTransfer;
  _resourceManager = resourceManager;
  _lightManager = lightManager;
  _state = state;
  _defaultMaterialPhong = std::make_shared<MaterialPhong>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
  _defaultMaterialPBR = std::make_shared<MaterialPBR>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
  _defaultMaterialColor = std::make_shared<MaterialColor>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
  _mesh = std::make_shared<Mesh3D>(state);
  _defaultAnimation = std::make_shared<Animation>(std::vector<std::shared_ptr<NodeGLTF>>{},
                                                  std::vector<std::shared_ptr<SkinGLTF>>{},
                                                  std::vector<std::shared_ptr<AnimationGLTF>>{}, state);

  auto layoutCamera = std::make_shared<DescriptorSetLayout>(state->getDevice());
  layoutCamera->createUniformBuffer();
  auto layoutCameraGeometry = std::make_shared<DescriptorSetLayout>(state->getDevice());
  layoutCameraGeometry->createUniformBuffer(VK_SHADER_STAGE_GEOMETRY_BIT);
  // layout for Color
  _descriptorSetLayout[MaterialType::COLOR].push_back({"camera", layoutCamera});
  _descriptorSetLayout[MaterialType::COLOR].push_back({"joint", _defaultAnimation->getDescriptorSetLayoutJoints()});
  _descriptorSetLayout[MaterialType::COLOR].push_back(
      {"texture", _defaultMaterialColor->getDescriptorSetLayoutTextures()});

  // layout for Phong
  _descriptorSetLayout[MaterialType::PHONG].push_back({"camera", layoutCamera});
  _descriptorSetLayout[MaterialType::PHONG].push_back({"joint", _defaultAnimation->getDescriptorSetLayoutJoints()});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"texture", _defaultMaterialPhong->getDescriptorSetLayoutTextures()});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"lightVP", _lightManager->getDSLViewProjection(VK_SHADER_STAGE_VERTEX_BIT)});
  _descriptorSetLayout[MaterialType::PHONG].push_back({"lightPhong", _lightManager->getDSLLightPhong()});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"alphaMask", _defaultMaterialPhong->getDescriptorSetLayoutAlphaCutoff()});
  _descriptorSetLayout[MaterialType::PHONG].push_back({"shadowTexture", _lightManager->getDSLShadowTexture()});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"materialCoefficients", _defaultMaterialPhong->getDescriptorSetLayoutCoefficients()});

  // layout for PBR
  _descriptorSetLayout[MaterialType::PBR].push_back({"camera", layoutCamera});
  _descriptorSetLayout[MaterialType::PBR].push_back({"joint", _defaultAnimation->getDescriptorSetLayoutJoints()});
  _descriptorSetLayout[MaterialType::PBR].push_back({"texture", _defaultMaterialPBR->getDescriptorSetLayoutTextures()});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {"lightVP", _lightManager->getDSLViewProjection(VK_SHADER_STAGE_VERTEX_BIT)});
  _descriptorSetLayout[MaterialType::PBR].push_back({"lightPBR", _lightManager->getDSLLightPBR()});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {"alphaMask", _defaultMaterialPBR->getDescriptorSetLayoutAlphaCutoff()});
  _descriptorSetLayout[MaterialType::PBR].push_back({"shadowTexture", _lightManager->getDSLShadowTexture()});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {"materialCoefficients", _defaultMaterialPBR->getDescriptorSetLayoutCoefficients()});

  // layout for Normal
  _descriptorSetLayoutNormal.push_back({"camera", layoutCamera});
  _descriptorSetLayoutNormal.push_back({"cameraGeometry", layoutCameraGeometry});
  // initialize Color
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("shaders/model/modelColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/model/modelColor_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[MaterialType::COLOR] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());

    _pipeline[MaterialType::COLOR]->createGraphic3D(renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
                                                    {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                     shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                    _descriptorSetLayout[MaterialType::COLOR], {},
                                                    _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());

    _pipelineCullOff[MaterialType::COLOR] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    _pipelineCullOff[MaterialType::COLOR]->createGraphic3D(renderFormat, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
                                                           {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                            shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                           _descriptorSetLayout[MaterialType::COLOR], {},
                                                           _mesh->getBindingDescription(),
                                                           _mesh->getAttributeDescriptions());

    _pipelineWireframe[MaterialType::COLOR] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    _pipelineWireframe[MaterialType::COLOR]->createGraphic3D(renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
                                                             {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                              shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                             _descriptorSetLayout[MaterialType::COLOR], {},
                                                             _mesh->getBindingDescription(),
                                                             _mesh->getAttributeDescriptions());
  }

  // initialize Phong
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("shaders/model/modelPhong_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/model/modelPhong_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

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

    _pipelineWireframe[MaterialType::PHONG] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    _pipelineWireframe[MaterialType::PHONG]->createGraphic3D(renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
                                                             {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                              shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                             _descriptorSetLayout[MaterialType::PHONG],
                                                             defaultPushConstants, _mesh->getBindingDescription(),
                                                             _mesh->getAttributeDescriptions());
  }
  // initialize PBR
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("shaders/model/modelPBR_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/model/modelPBR_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

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

    _pipelineWireframe[MaterialType::PBR] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    _pipelineWireframe[MaterialType::PBR]->createGraphic3D(renderFormat, VK_CULL_MODE_NONE, VK_POLYGON_MODE_LINE,
                                                           {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                            shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                           _descriptorSetLayout[MaterialType::PBR],
                                                           defaultPushConstants, _mesh->getBindingDescription(),
                                                           _mesh->getAttributeDescriptions());
  }

  // initialize Normal (per vertex)
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("shaders/shape/cubeNormal_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/shape/cubeNormal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    shader->add("shaders/shape/cubeNormal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

    _pipelineNormalMesh = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineNormalMesh->createGraphic3D(renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
                                         {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                          shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
                                          shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
                                         _descriptorSetLayoutNormal, {}, _mesh->getBindingDescription(),
                                         _mesh->getAttributeDescriptions());

    // it's kind of pointless, but if material is set, double sided property can be handled
    _pipelineNormalMeshCullOff = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineNormalMeshCullOff->createGraphic3D(renderFormat, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
                                                {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                 shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
                                                 shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
                                                _descriptorSetLayoutNormal, {}, _mesh->getBindingDescription(),
                                                _mesh->getAttributeDescriptions());
  }

  // initialize Tangent (per vertex)
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("shaders/shape/cubeTangent_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/shape/cubeNormal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    shader->add("shaders/shape/cubeNormal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

    _pipelineTangentMesh = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineTangentMesh->createGraphic3D(renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
                                          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
                                           shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
                                          _descriptorSetLayoutNormal, {}, _mesh->getBindingDescription(),
                                          _mesh->getAttributeDescriptions());

    // it's kind of pointless, but if material is set, double sided property can be handled
    _pipelineTangentMeshCullOff = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineTangentMeshCullOff->createGraphic3D(renderFormat, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
                                                 {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                  shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
                                                  shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
                                                 _descriptorSetLayoutNormal, {}, _mesh->getBindingDescription(),
                                                 _mesh->getAttributeDescriptions());
  }

  // initialize depth directional
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("shaders/model/modelDepth_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    _pipelineDirectional = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineDirectional->createGraphic3DShadow(
        VK_CULL_MODE_NONE, {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT)},
        {{"camera", layoutCamera}, {"joint", _defaultAnimation->getDescriptorSetLayoutJoints()}}, {},
        _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }

  // initialize depth point
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("shaders/model/modelDepth_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/model/modelDepth_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelinePoint = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["fragment"] = DepthConstants::getPushConstant(0);
    _pipelinePoint->createGraphic3DShadow(
        VK_CULL_MODE_NONE,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        {{"camera", layoutCamera}, {"joint", _defaultAnimation->getDescriptorSetLayoutJoints()}}, defaultPushConstants,
        _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }
}

std::shared_ptr<Model3D> Model3DManager::createModel3D(const std::vector<std::shared_ptr<NodeGLTF>>& nodes,
                                                       const std::vector<std::shared_ptr<Mesh3D>>& meshes) {
  auto model = std::make_shared<Model3D>(nodes, meshes, _commandBufferTransfer, _resourceManager, _state);
  model->setAnimation(_defaultAnimation);
  return model;
}
void Model3DManager::registerModel3D(std::shared_ptr<Model3D> model) {
  if (std::find(_modelsGLTF.begin(), _modelsGLTF.end(), model) == _modelsGLTF.end()) _modelsGLTF.push_back(model);
}

void Model3DManager::unregisterModel3D(std::shared_ptr<Model3D> model) {
  _modelsGLTF.erase(std::remove(_modelsGLTF.begin(), _modelsGLTF.end(), model), _modelsGLTF.end());
}

// todo: choose appropriate pipeline using material from sprite
void Model3DManager::draw(std::tuple<int, int> resolution,
                          std::shared_ptr<Camera> camera,
                          std::shared_ptr<CommandBuffer> commandBuffer) {
  int currentFrame = _state->getFrameInFlight();
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = std::get<1>(resolution);
  viewport.width = std::get<0>(resolution);
  viewport.height = -std::get<1>(resolution);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(std::get<0>(resolution), std::get<1>(resolution));
  vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  auto drawModel = [&](MaterialType materialType, DrawType drawType) {
    auto pipeline = _pipeline[materialType];
    auto pipelineCullOff = _pipelineCullOff[materialType];
    if (drawType == DrawType::WIREFRAME) {
      pipeline = _pipelineWireframe[materialType];
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
          pipeline->getPipelineLayout(), 3, 1,
          &_lightManager->getDSViewProjection(VK_SHADER_STAGE_VERTEX_BIT)->getDescriptorSets()[currentFrame], 0,
          nullptr);
    }

    auto lightLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                    [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                      return info.first == std::string("lightPhong");
                                    });
    if (lightLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 4, 1,
                              &_lightManager->getDSLightPhong()->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    lightLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                               [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                 return info.first == std::string("lightPBR");
                               });
    if (lightLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 4, 1,
                              &_lightManager->getDSLightPBR()->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    auto shadowTextureLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                            [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                              return info.first == "shadowTexture";
                                            });
    if (shadowTextureLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 6, 1,
                              &_lightManager->getDSShadowTexture()[currentFrame]->getDescriptorSets()[currentFrame], 0,
                              nullptr);
    }

    for (auto model : _modelsGLTF) {
      if (model->getMaterialType() == materialType && model->getDrawType() == drawType) {
        model->draw(camera, commandBuffer, pipeline, pipelineCullOff);
      }
    }
  };

  auto drawModelNormal = [&](DrawType drawType) {
    auto pipeline = _pipelineNormalMesh;
    auto pipelineCullOff = _pipelineNormalMeshCullOff;

    if (drawType == DrawType::TANGENT) {
      pipeline = _pipelineTangentMesh;
      pipelineCullOff = _pipelineTangentMeshCullOff;
    }

    auto pipelineLayout = pipeline->getDescriptorSetLayout();
    vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline->getPipeline());

    for (auto model : _modelsGLTF) {
      if (model->getDrawType() == drawType) {
        model->draw(camera, commandBuffer, pipeline, pipelineCullOff);
      }
    }
  };

  drawModel(MaterialType::PHONG, DrawType::FILL);
  drawModel(MaterialType::PHONG, DrawType::WIREFRAME);
  drawModel(MaterialType::PBR, DrawType::FILL);
  drawModel(MaterialType::PBR, DrawType::WIREFRAME);
  drawModel(MaterialType::COLOR, DrawType::FILL);
  drawModel(MaterialType::COLOR, DrawType::WIREFRAME);
  drawModelNormal(DrawType::NORMAL);
  drawModelNormal(DrawType::TANGENT);
}

void Model3DManager::drawShadow(LightType lightType,
                                int lightIndex,
                                int face,
                                std::shared_ptr<CommandBuffer> commandBuffer) {
  int currentFrame = _state->getFrameInFlight();
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
      model->drawShadow(lightIndexTotal, view, projection, face, commandBuffer, pipeline, pipeline);
    }
    if (lightType == LightType::POINT) {
      lightIndexTotal += _state->getSettings()->getMaxDirectionalLights();
      view = _lightManager->getPointLights()[lightIndex]->getViewMatrix(face);
      projection = _lightManager->getPointLights()[lightIndex]->getProjectionMatrix();
      model->drawShadow(lightIndexTotal, view, projection, face, commandBuffer, pipeline, pipeline);
    }
  }
}