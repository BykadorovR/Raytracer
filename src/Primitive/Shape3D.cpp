#include "Shape3D.h"
#undef far

Shape3D::Shape3D(ShapeType shapeType,
                 std::vector<VkFormat> renderFormat,
                 VkCullModeFlags cullMode,
                 std::shared_ptr<LightManager> lightManager,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<ResourceManager> resourceManager,
                 std::shared_ptr<State> state) {
  _shapeType = shapeType;
  _state = state;
  _lightManager = lightManager;

  // needed for layout
  _defaultMaterialColor = std::make_shared<MaterialColor>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
  _defaultMaterialPhong = std::make_shared<MaterialPhong>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
  _defaultMaterialPBR = std::make_shared<MaterialPBR>(MaterialTarget::SIMPLE, commandBufferTransfer, state);

  if (shapeType == ShapeType::CUBE) {
    _mesh = std::make_shared<MeshCube>(commandBufferTransfer, state);
    _defaultMaterialColor->setBaseColor({resourceManager->getCubemapOne()->getTexture()});
    _shadersColor[ShapeType::CUBE][MaterialType::COLOR] = {"shaders/shape/cubeColor_vertex.spv",
                                                           "shaders/shape/cubeColor_fragment.spv"};
    _shadersColor[ShapeType::CUBE][MaterialType::PHONG] = {"shaders/shape/cubePhong_vertex.spv",
                                                           "shaders/shape/cubePhong_fragment.spv"};
    _shadersColor[ShapeType::CUBE][MaterialType::PBR] = {"shaders/shape/cubePhong_vertex.spv",
                                                         "shaders/shape/cubePBR_fragment.spv"};
    _shadersLight[ShapeType::CUBE] = {"shaders/shape/cubeDepth_vertex.spv", "shaders/shape/cubeDepth_fragment.spv"};
    _shadersNormalsMesh[ShapeType::CUBE] = {"shaders/shape/cubeNormal_vertex.spv",
                                            "shaders/shape/cubeNormal_fragment.spv",
                                            "shaders/shape/cubeNormal_geometry.spv"};
    _shadersTangentMesh[ShapeType::CUBE] = {"shaders/shape/cubeTangent_vertex.spv",
                                            "shaders/shape/cubeNormal_fragment.spv",
                                            "shaders/shape/cubeNormal_geometry.spv"};
  }

  if (shapeType == ShapeType::SPHERE) {
    _mesh = std::make_shared<MeshSphere>(commandBufferTransfer, state);
    _defaultMaterialColor->setBaseColor({resourceManager->getTextureOne()});
    _shadersColor[ShapeType::SPHERE][MaterialType::COLOR] = {"shaders/shape/sphereColor_vertex.spv",
                                                             "shaders/shape/sphereColor_fragment.spv"};
    _shadersColor[ShapeType::SPHERE][MaterialType::PHONG] = {"shaders/shape/spherePhong_vertex.spv",
                                                             "shaders/shape/spherePhong_fragment.spv"};
    _shadersColor[ShapeType::SPHERE][MaterialType::PBR] = {"shaders/shape/spherePhong_vertex.spv",
                                                           "shaders/shape/spherePBR_fragment.spv"};
    _shadersLight[ShapeType::SPHERE] = {"shaders/shape/sphereDepth_vertex.spv",
                                        "shaders/shape/sphereDepth_fragment.spv"};
    _shadersNormalsMesh[ShapeType::SPHERE] = {"shaders/shape/cubeNormal_vertex.spv",
                                              "shaders/shape/cubeNormal_fragment.spv",
                                              "shaders/shape/cubeNormal_geometry.spv"};
    _shadersTangentMesh[ShapeType::SPHERE] = {"shaders/shape/cubeTangent_vertex.spv",
                                              "shaders/shape/cubeNormal_fragment.spv",
                                              "shaders/shape/cubeNormal_geometry.spv"};
  }

  _material = _defaultMaterialColor;
  // initialize camera UBO and descriptor sets for draw
  // initialize UBO
  _uniformBufferCamera = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(),
                                                         sizeof(BufferMVP), state);
  auto layoutCamera = std::make_shared<DescriptorSetLayout>(state->getDevice());
  layoutCamera->createUniformBuffer();
  auto layoutCameraGeometry = std::make_shared<DescriptorSetLayout>(state->getDevice());
  layoutCameraGeometry->createUniformBuffer(VK_SHADER_STAGE_GEOMETRY_BIT);

  // layout for Color
  _descriptorSetLayout[MaterialType::COLOR].push_back({std::string("camera"), layoutCamera});
  _descriptorSetLayout[MaterialType::COLOR].push_back(
      {std::string("texture"), _defaultMaterialColor->getDescriptorSetLayoutTextures()});
  // layout for Phong
  _descriptorSetLayout[MaterialType::PHONG].push_back({"camera", layoutCamera});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"texture", _defaultMaterialPhong->getDescriptorSetLayoutTextures()});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"lightVP", _lightManager->getDSLViewProjection(VK_SHADER_STAGE_VERTEX_BIT)});
  _descriptorSetLayout[MaterialType::PHONG].push_back({"lightPhong", _lightManager->getDSLLightPhong()});
  _descriptorSetLayout[MaterialType::PHONG].push_back({"shadowTexture", _lightManager->getDSLShadowTexture()});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"materialCoefficients", _defaultMaterialPhong->getDescriptorSetLayoutCoefficients()});
  // layout for PBR
  _descriptorSetLayout[MaterialType::PBR].push_back({"camera", layoutCamera});
  _descriptorSetLayout[MaterialType::PBR].push_back({"texture", _defaultMaterialPBR->getDescriptorSetLayoutTextures()});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {"lightVP", _lightManager->getDSLViewProjection(VK_SHADER_STAGE_VERTEX_BIT)});
  _descriptorSetLayout[MaterialType::PBR].push_back({"lightPBR", _lightManager->getDSLLightPBR()});
  _descriptorSetLayout[MaterialType::PBR].push_back({"shadowTexture", _lightManager->getDSLShadowTexture()});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {"materialCoefficients", _defaultMaterialPBR->getDescriptorSetLayoutCoefficients()});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {"alphaMask", _defaultMaterialPBR->getDescriptorSetLayoutAlphaCutoff()});

  // layout for Normals/Tangents (per vertex)
  _descriptorSetLayoutNormalsMesh.push_back({"camera", layoutCamera});
  _descriptorSetLayoutNormalsMesh.push_back({"cameraGeometry", layoutCameraGeometry});

  _descriptorSetCamera = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(), layoutCamera,
                                                         state->getDescriptorPool(), state->getDevice());
  _descriptorSetCamera->createUniformBuffer(_uniformBufferCamera);

  _descriptorSetCameraGeometry = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                                 layoutCameraGeometry, state->getDescriptorPool(),
                                                                 state->getDevice());
  _descriptorSetCameraGeometry->createUniformBuffer(_uniformBufferCamera);
  // initialize camera UBO and descriptor sets for shadow
  // initialize UBO
  for (int i = 0; i < _state->getSettings()->getMaxDirectionalLights(); i++) {
    _cameraUBODepth.push_back(
        {std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(BufferMVP), _state)});
  }

  for (int i = 0; i < _state->getSettings()->getMaxPointLights(); i++) {
    std::vector<std::shared_ptr<UniformBuffer>> facesBuffer(6);
    for (int j = 0; j < 6; j++) {
      facesBuffer[j] = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(BufferMVP),
                                                       _state);
    }
    _cameraUBODepth.push_back(facesBuffer);
  }
  auto cameraDescriptorSetLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
  cameraDescriptorSetLayout->createUniformBuffer();
  {
    for (int i = 0; i < _state->getSettings()->getMaxDirectionalLights(); i++) {
      auto cameraSet = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                       cameraDescriptorSetLayout, _state->getDescriptorPool(),
                                                       _state->getDevice());
      cameraSet->createUniformBuffer(_cameraUBODepth[i][0]);

      _descriptorSetCameraDepth.push_back({cameraSet});
    }

    for (int i = 0; i < _state->getSettings()->getMaxPointLights(); i++) {
      std::vector<std::shared_ptr<DescriptorSet>> facesSet(6);
      for (int j = 0; j < 6; j++) {
        facesSet[j] = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                      cameraDescriptorSetLayout, _state->getDescriptorPool(),
                                                      _state->getDevice());
        facesSet[j]->createUniformBuffer(_cameraUBODepth[i + _state->getSettings()->getMaxDirectionalLights()][j]);
      }
      _descriptorSetCameraDepth.push_back(facesSet);
    }
  }
  // initialize Color
  {
    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add(_shadersColor[_shapeType][MaterialType::COLOR][0], VK_SHADER_STAGE_VERTEX_BIT);
    shader->add(_shadersColor[_shapeType][MaterialType::COLOR][1], VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipeline[MaterialType::COLOR] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipeline[MaterialType::COLOR]->createGraphic3D(renderFormat, cullMode, VK_POLYGON_MODE_FILL,
                                                    {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                     shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                    _descriptorSetLayout[MaterialType::COLOR], {},
                                                    _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
    _pipelineWireframe[MaterialType::COLOR] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineWireframe[MaterialType::COLOR]->createGraphic3D(renderFormat, cullMode, VK_POLYGON_MODE_LINE,
                                                             {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                              shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                             _descriptorSetLayout[MaterialType::COLOR], {},
                                                             _mesh->getBindingDescription(),
                                                             _mesh->getAttributeDescriptions());
  }
  // initialize Phong
  {
    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add(_shadersColor[_shapeType][MaterialType::PHONG][0], VK_SHADER_STAGE_VERTEX_BIT);
    shader->add(_shadersColor[_shapeType][MaterialType::PHONG][1], VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[MaterialType::PHONG] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipeline[MaterialType::PHONG]->createGraphic3D(
        renderFormat, cullMode, VK_POLYGON_MODE_FILL,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        _descriptorSetLayout[MaterialType::PHONG],
        std::map<std::string, VkPushConstantRange>{{std::string("fragment"), LightPush::getPushConstant()}},
        _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
    // wireframe one
    _pipelineWireframe[MaterialType::PHONG] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineWireframe[MaterialType::PHONG]->createGraphic3D(
        renderFormat, cullMode, VK_POLYGON_MODE_LINE,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        _descriptorSetLayout[MaterialType::PHONG],
        std::map<std::string, VkPushConstantRange>{{std::string("fragment"), LightPush::getPushConstant()}},
        _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }
  // initialize PBR
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add(_shadersColor[_shapeType][MaterialType::PBR][0], VK_SHADER_STAGE_VERTEX_BIT);
    shader->add(_shadersColor[_shapeType][MaterialType::PBR][1], VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[MaterialType::PBR] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipeline[MaterialType::PBR]->createGraphic3D(
        renderFormat, cullMode, VK_POLYGON_MODE_FILL,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        _descriptorSetLayout[MaterialType::PBR],
        std::map<std::string, VkPushConstantRange>{{std::string("fragment"), LightPush::getPushConstant()}},
        _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
    // wireframe one
    _pipelineWireframe[MaterialType::PBR] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineWireframe[MaterialType::PBR]->createGraphic3D(
        renderFormat, cullMode, VK_POLYGON_MODE_LINE,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        _descriptorSetLayout[MaterialType::PBR],
        std::map<std::string, VkPushConstantRange>{{std::string("fragment"), LightPush::getPushConstant()}},
        _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }

  // initialize Normal (per vertex)
  {
    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add(_shadersNormalsMesh[_shapeType][0], VK_SHADER_STAGE_VERTEX_BIT);
    shader->add(_shadersNormalsMesh[_shapeType][1], VK_SHADER_STAGE_FRAGMENT_BIT);
    shader->add(_shadersNormalsMesh[_shapeType][2], VK_SHADER_STAGE_GEOMETRY_BIT);

    _pipelineNormalMesh = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineNormalMesh->createGraphic3D(renderFormat, cullMode, VK_POLYGON_MODE_FILL,
                                         {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                          shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
                                          shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
                                         _descriptorSetLayoutNormalsMesh, {}, _mesh->getBindingDescription(),
                                         _mesh->getAttributeDescriptions());
  }
  // initialize Tangent (per vertex)
  {
    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add(_shadersTangentMesh[_shapeType][0], VK_SHADER_STAGE_VERTEX_BIT);
    shader->add(_shadersTangentMesh[_shapeType][1], VK_SHADER_STAGE_FRAGMENT_BIT);
    shader->add(_shadersTangentMesh[_shapeType][2], VK_SHADER_STAGE_GEOMETRY_BIT);

    _pipelineTangentMesh = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineTangentMesh->createGraphic3D(renderFormat, cullMode, VK_POLYGON_MODE_FILL,
                                          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
                                           shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
                                          _descriptorSetLayoutNormalsMesh, {}, _mesh->getBindingDescription(),
                                          _mesh->getAttributeDescriptions());
  }
  // initialize shadows directional
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add(_shadersLight[_shapeType][0], VK_SHADER_STAGE_VERTEX_BIT);
    _pipelineDirectional = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineDirectional->createGraphic3DShadow(
        VK_CULL_MODE_NONE, {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT)}, {{"camera", layoutCamera}}, {},
        _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }

  // initialize shadows point
  {
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["fragment"] = DepthConstants::getPushConstant(0);

    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add(_shadersLight[_shapeType][0], VK_SHADER_STAGE_VERTEX_BIT);
    shader->add(_shadersLight[_shapeType][1], VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelinePoint = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelinePoint->createGraphic3DShadow(VK_CULL_MODE_NONE,
                                          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                          {{"camera", layoutCamera}}, defaultPushConstants,
                                          _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }
}

void Shape3D::enableShadow(bool enable) { _enableShadow = enable; }

void Shape3D::enableLighting(bool enable) { _enableLighting = enable; }

void Shape3D::setMaterial(std::shared_ptr<MaterialColor> material) {
  _material = material;
  _materialType = MaterialType::COLOR;
}

void Shape3D::setMaterial(std::shared_ptr<MaterialPhong> material) {
  _material = material;
  _materialType = MaterialType::PHONG;
}

void Shape3D::setMaterial(std::shared_ptr<MaterialPBR> material) {
  _material = material;
  _materialType = MaterialType::PBR;
}

void Shape3D::setDrawType(DrawType drawType) { _drawType = drawType; }

std::shared_ptr<Mesh3D> Shape3D::getMesh() { return _mesh; }

void Shape3D::draw(std::tuple<int, int> resolution,
                   std::shared_ptr<Camera> camera,
                   std::shared_ptr<CommandBuffer> commandBuffer) {
  int currentFrame = _state->getFrameInFlight();
  auto drawShape3D = [&](std::shared_ptr<Pipeline> pipeline) {
    vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline->getPipeline());
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

    // light push constants
    if (pipeline->getPushConstants().find("fragment") != pipeline->getPushConstants().end()) {
      LightPush pushConstants;
      pushConstants.enableShadow = _enableShadow;
      pushConstants.enableLighting = _enableLighting;
      pushConstants.cameraPosition = camera->getEye();

      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LightPush), &pushConstants);
    }

    BufferMVP cameraUBO{};
    cameraUBO.model = _model;
    cameraUBO.view = camera->getView();
    cameraUBO.projection = camera->getProjection();

    void* data;
    vkMapMemory(_state->getDevice()->getLogicalDevice(), _uniformBufferCamera->getBuffer()[currentFrame]->getMemory(),
                0, sizeof(cameraUBO), 0, &data);
    memcpy(data, &cameraUBO, sizeof(cameraUBO));
    vkUnmapMemory(_state->getDevice()->getLogicalDevice(),
                  _uniformBufferCamera->getBuffer()[currentFrame]->getMemory());

    VkBuffer vertexBuffers[] = {_mesh->getVertexBuffer()->getBuffer()->getData()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame],
                         _mesh->getIndexBuffer()->getBuffer()->getData(), 0, VK_INDEX_TYPE_UINT32);

    // camera
    auto pipelineLayout = pipeline->getDescriptorSetLayout();
    auto cameraLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                     [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                       return info.first == std::string("camera");
                                     });
    if (cameraLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 0, 1,
                              &_descriptorSetCamera->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    // camera geometry
    auto cameraLayoutGeometry = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                             [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                               return info.first == std::string("cameraGeometry");
                                             });
    if (cameraLayoutGeometry != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 1, 1,
                              &_descriptorSetCameraGeometry->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    // textures
    auto textureLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                        return info.first == std::string("texture");
                                      });
    if (textureLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 1, 1,
                              &_material->getDescriptorSetTextures(currentFrame)->getDescriptorSets()[currentFrame], 0,
                              nullptr);
    }

    // coefficients
    auto materialCoefficientsLayout = std::find_if(
        pipelineLayout.begin(), pipelineLayout.end(),
        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
          return info.first == std::string("materialCoefficients");
        });
    if (materialCoefficientsLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 5, 1,
                              &_material->getDescriptorSetCoefficients(currentFrame)->getDescriptorSets()[currentFrame],
                              0, nullptr);
    }

    // alpha mask
    auto alphaMaskLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                          return info.first == std::string("alphaMask");
                                        });
    if (alphaMaskLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 6, 1,
                              &_material->getDescriptorSetAlphaCutoff()->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    // light view projection
    auto lightVPLayout = std::find_if(
        pipelineLayout.begin(), pipelineLayout.end(),
        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "lightVP"; });
    if (lightVPLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(
          commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
          pipeline->getPipelineLayout(), 2, 1,
          &_lightManager->getDSViewProjection(VK_SHADER_STAGE_VERTEX_BIT)->getDescriptorSets()[currentFrame], 0,
          nullptr);
    }

    // lights
    auto lightLayout = std::find_if(
        pipelineLayout.begin(), pipelineLayout.end(),
        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "lightPhong"; });
    if (lightLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 3, 1,
                              &_lightManager->getDSLightPhong()->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    lightLayout = std::find_if(
        pipelineLayout.begin(), pipelineLayout.end(),
        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "lightPBR"; });
    if (lightLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 3, 1,
                              &_lightManager->getDSLightPBR()->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    // depth texture
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

    vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame],
                     static_cast<uint32_t>(_mesh->getIndexData().size()), 1, 0, 0, 0);
  };

  auto pipeline = _pipeline[_materialType];
  if (_drawType == DrawType::WIREFRAME) pipeline = _pipelineWireframe[_materialType];
  if (_drawType == DrawType::NORMAL) pipeline = _pipelineNormalMesh;
  if (_drawType == DrawType::TANGENT) pipeline = _pipelineTangentMesh;
  drawShape3D(pipeline);
}

void Shape3D::drawShadow(LightType lightType, int lightIndex, int face, std::shared_ptr<CommandBuffer> commandBuffer) {
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
  // if we swap, we need to change shader as well, so swap there. But we can't do it there because we sample from
  // cubemap and we can't just (1 - y)
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

  glm::mat4 view(1.f);
  glm::mat4 projection(1.f);
  int lightIndexTotal = lightIndex;
  if (lightType == LightType::DIRECTIONAL) {
    view = _lightManager->getDirectionalLights()[lightIndex]->getViewMatrix();
    projection = _lightManager->getDirectionalLights()[lightIndex]->getProjectionMatrix();
  }
  if (lightType == LightType::POINT) {
    lightIndexTotal += _state->getSettings()->getMaxDirectionalLights();
    view = _lightManager->getPointLights()[lightIndex]->getViewMatrix(face);
    projection = _lightManager->getPointLights()[lightIndex]->getProjectionMatrix();
  }

  BufferMVP cameraMVP{};
  cameraMVP.model = _model;
  cameraMVP.view = view;
  cameraMVP.projection = projection;

  void* data;
  vkMapMemory(_state->getDevice()->getLogicalDevice(),
              _cameraUBODepth[lightIndexTotal][face]->getBuffer()[currentFrame]->getMemory(), 0, sizeof(cameraMVP), 0,
              &data);
  memcpy(data, &cameraMVP, sizeof(cameraMVP));
  vkUnmapMemory(_state->getDevice()->getLogicalDevice(),
                _cameraUBODepth[lightIndexTotal][face]->getBuffer()[currentFrame]->getMemory());

  VkBuffer vertexBuffers[] = {_mesh->getVertexBuffer()->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame], _mesh->getIndexBuffer()->getBuffer()->getData(),
                       0, VK_INDEX_TYPE_UINT32);

  auto pipelineLayout = pipeline->getDescriptorSetLayout();
  auto cameraLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                   [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                     return info.first == std::string("camera");
                                   });
  if (cameraLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(),
        0, 1, &_descriptorSetCameraDepth[lightIndexTotal][face]->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_mesh->getIndexData().size()),
                   1, 0, 0, 0);
}