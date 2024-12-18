#include "Primitive/Sprite.h"
#undef far

struct FragmentPointLightPushDepth {
  alignas(16) glm::vec3 lightPosition;
  alignas(16) int far;
};

struct FragmentPush {
  int enableShadow;
  int enableLighting;
  alignas(16) glm::vec3 cameraPosition;
};

Sprite::Sprite(std::shared_ptr<CommandBuffer> commandBufferTransfer,
               std::shared_ptr<GameState> gameState,
               std::shared_ptr<EngineState> engineState) {
  setName("Sprite");
  _engineState = engineState;
  _gameState = gameState;
  auto loggerUtils = std::make_shared<LoggerUtils>(_engineState);
  auto settings = engineState->getSettings();
  // default material if model doesn't have material at all, we still have to send data to shader
  _defaultMaterialPhong = std::make_shared<MaterialPhong>(MaterialTarget::SIMPLE, commandBufferTransfer, engineState);
  _defaultMaterialPhong->setBaseColor({_gameState->getResourceManager()->getTextureOne()});
  _defaultMaterialPhong->setNormal({_gameState->getResourceManager()->getTextureZero()});
  _defaultMaterialPhong->setSpecular({_gameState->getResourceManager()->getTextureZero()});
  _defaultMaterialPBR = std::make_shared<MaterialPBR>(MaterialTarget::SIMPLE, commandBufferTransfer, engineState);
  _defaultMaterialColor = std::make_shared<MaterialColor>(MaterialTarget::SIMPLE, commandBufferTransfer, engineState);
  _material = _defaultMaterialPhong;
  _changedMaterialRender.resize(engineState->getSettings()->getMaxFramesInFlight());
  _changedMaterialShadow.resize(engineState->getSettings()->getMaxFramesInFlight());
  _mesh = std::make_shared<MeshStatic2D>(engineState);
  // 3   0
  // 2   1
  _mesh->setVertices({Vertex2D{{0.5f, 0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}, {1.f, 0.f, 0.f, 1.f}},
                      Vertex2D{{0.5f, -0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}, {1.f, 0.f, 0.f, 1.f}},
                      Vertex2D{{-0.5f, -0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}, {1.f, 0.f, 0.f, 1.f}},
                      Vertex2D{{-0.5f, 0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}, {1.f, 0.f, 0.f, 1.f}}},
                     commandBufferTransfer);
  _mesh->setIndexes({0, 3, 2, 2, 1, 0}, commandBufferTransfer);

  _renderPass = _engineState->getRenderPassManager()->getRenderPass(RenderPassScenario::GRAPHIC);
  _renderPassDepth = _engineState->getRenderPassManager()->getRenderPass(RenderPassScenario::SHADOW);

  // initialize UBO
  _cameraUBOFull.resize(_engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++)
    _cameraUBOFull[i] = std::make_shared<Buffer>(
        sizeof(BufferMVP), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);

  // setup Normal
  {
    _descriptorSetLayoutNormalsMesh = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutNormalsMesh{{.binding = 0,
                                                                 .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                                 .descriptorCount = 1,
                                                                 .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                                                 .pImmutableSamplers = nullptr},
                                                                {.binding = 1,
                                                                 .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                                 .descriptorCount = 1,
                                                                 .stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT,
                                                                 .pImmutableSamplers = nullptr}};
    _descriptorSetLayoutNormalsMesh->createCustom(layoutNormalsMesh);

    // TODO: we can just have one buffer and put it twice to descriptor
    _descriptorSetNormalsMesh = std::make_shared<DescriptorSet>(engineState->getSettings()->getMaxFramesInFlight(),
                                                                _descriptorSetLayoutNormalsMesh, engineState);
    loggerUtils->setName("Descriptor set sprite normals mesh", VkObjectType::VK_OBJECT_TYPE_DESCRIPTOR_SET,
                         _descriptorSetNormalsMesh->getDescriptorSets());
    for (int i = 0; i < engineState->getSettings()->getMaxFramesInFlight(); i++) {
      std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoNormalsMesh = {
          {0, {{.buffer = _cameraUBOFull[i]->getData(), .offset = 0, .range = _cameraUBOFull[i]->getSize()}}},
          {1, {{.buffer = _cameraUBOFull[i]->getData(), .offset = 0, .range = _cameraUBOFull[i]->getSize()}}}};
      _descriptorSetNormalsMesh->createCustom(i, bufferInfoNormalsMesh, {});
    }

    // initialize Normal (per vertex)
    {
      auto shader = std::make_shared<Shader>(_engineState);
      shader->add("shaders/sprite/spriteNormal_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/shape/cubeNormal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shader->add("shaders/shape/cubeNormal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

      _pipelineNormal = std::make_shared<PipelineGraphic>(_engineState->getDevice());
      _pipelineNormal->setDepthTest(true);
      _pipelineNormal->setDepthWrite(true);
      _pipelineNormal->setCullMode(VK_CULL_MODE_BACK_BIT);
      _pipelineNormal->createCustom(
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          {{"normal", _descriptorSetLayoutNormalsMesh}}, {}, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex2D, pos)},
                                                 {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex2D, normal)},
                                                 {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex2D, color)}}),
          _renderPass);
    }

    // initialize Tangent (per vertex)
    {
      auto shader = std::make_shared<Shader>(_engineState);
      shader->add("shaders/sprite/spriteTangent_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/shape/cubeNormal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shader->add("shaders/shape/cubeNormal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

      _pipelineTangent = std::make_shared<PipelineGraphic>(_engineState->getDevice());
      _pipelineTangent->setDepthTest(true);
      _pipelineTangent->setDepthWrite(true);
      _pipelineTangent->setCullMode(VK_CULL_MODE_BACK_BIT);
      _pipelineTangent->createCustom(
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          {{"normal", _descriptorSetLayoutNormalsMesh}}, {}, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex2D, pos)},
                                                 {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex2D, color)},
                                                 {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex2D, tangent)}}),
          _renderPass);
    }
  }

  // setup color
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutColor{{.binding = 0,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                                           .pImmutableSamplers = nullptr},
                                                          {.binding = 1,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                           .pImmutableSamplers = nullptr}};

    descriptorSetLayout->createCustom(layoutColor);
    _descriptorSetLayout[MaterialType::COLOR].push_back({"color", descriptorSetLayout});
    // depth has the same layout
    _descriptorSetLayoutDepth = descriptorSetLayout;

    _descriptorSetColor = std::make_shared<DescriptorSet>(engineState->getSettings()->getMaxFramesInFlight(),
                                                          descriptorSetLayout, engineState);
    loggerUtils->setName("Descriptor set sprite color", VkObjectType::VK_OBJECT_TYPE_DESCRIPTOR_SET,
                         _descriptorSetColor->getDescriptorSets());

    // phong is default, will form default descriptor only for it here
    // descriptors are formed in setMaterial

    // initialize Color
    {
      auto shader = std::make_shared<Shader>(_engineState);
      shader->add("shaders/sprite/spriteColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/sprite/spriteColor_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

      _pipeline[MaterialType::COLOR] = std::make_shared<PipelineGraphic>(_engineState->getDevice());
      _pipeline[MaterialType::COLOR]->setDepthTest(true);
      _pipeline[MaterialType::COLOR]->setDepthWrite(true);
      _pipeline[MaterialType::COLOR]->setCullMode(VK_CULL_MODE_BACK_BIT);
      _pipeline[MaterialType::COLOR]->createCustom(
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          _descriptorSetLayout[MaterialType::COLOR], {}, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex2D, pos)},
                                                 {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex2D, color)},
                                                 {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex2D, texCoord)}}),

          _renderPass);
      // wireframe one
      _pipelineWireframe[MaterialType::COLOR] = std::make_shared<PipelineGraphic>(_engineState->getDevice());
      _pipelineWireframe[MaterialType::COLOR]->setDepthTest(true);
      _pipelineWireframe[MaterialType::COLOR]->setDepthWrite(true);
      _pipelineWireframe[MaterialType::COLOR]->setCullMode(VK_CULL_MODE_BACK_BIT);
      _pipelineWireframe[MaterialType::COLOR]->setPolygonMode(VK_POLYGON_MODE_LINE);
      _pipelineWireframe[MaterialType::COLOR]->setAlphaBlending(false);
      _pipelineWireframe[MaterialType::COLOR]->createCustom(
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          _descriptorSetLayout[MaterialType::COLOR], {}, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex2D, pos)},
                                                 {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex2D, color)},
                                                 {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex2D, texCoord)}}),
          _renderPass);
    }
  }

  // initialize camera UBO and descriptor sets for shadow
  // initialize UBO
  for (int i = 0; i < _engineState->getSettings()->getMaxDirectionalLights(); i++) {
    std::vector<std::shared_ptr<Buffer>> buffer(_engineState->getSettings()->getMaxFramesInFlight());
    for (int j = 0; j < _engineState->getSettings()->getMaxFramesInFlight(); j++)
      buffer[j] = std::make_shared<Buffer>(sizeof(BufferMVP), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                           _engineState);
    _cameraUBODepth.push_back({buffer});
    auto cameraSet = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                     _descriptorSetLayoutDepth, _engineState);
    loggerUtils->setName("Descriptor set sprite directional camera " + std::to_string(i),
                         VkObjectType::VK_OBJECT_TYPE_DESCRIPTOR_SET, cameraSet->getDescriptorSets());
    _descriptorSetCameraDepth.push_back({cameraSet});
  }

  for (int i = 0; i < _engineState->getSettings()->getMaxPointLights(); i++) {
    std::vector<std::vector<std::shared_ptr<Buffer>>> facesBuffer(6);
    std::vector<std::shared_ptr<DescriptorSet>> facesSet(6);
    for (int j = 0; j < 6; j++) {
      facesBuffer[j].resize(_engineState->getSettings()->getMaxFramesInFlight());
      for (int k = 0; k < _engineState->getSettings()->getMaxFramesInFlight(); k++)
        facesBuffer[j][k] = std::make_shared<Buffer>(
            sizeof(BufferMVP), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);
      facesSet[j] = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                    _descriptorSetLayoutDepth, _engineState);
      loggerUtils->setName("Descriptor set sprite point camera " + std::to_string(i) + "x" + std::to_string(j),
                           VkObjectType::VK_OBJECT_TYPE_DESCRIPTOR_SET, facesSet[j]->getDescriptorSets());
    }
    _cameraUBODepth.push_back(facesBuffer);
    _descriptorSetCameraDepth.push_back(facesSet);
  }

  // setup Phong
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutPhong{{.binding = 0,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                                           .pImmutableSamplers = nullptr},
                                                          {.binding = 1,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                           .pImmutableSamplers = nullptr},
                                                          {.binding = 2,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                           .pImmutableSamplers = nullptr},
                                                          {.binding = 3,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                           .pImmutableSamplers = nullptr},
                                                          {.binding = 4,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                           .pImmutableSamplers = nullptr}};
    descriptorSetLayout->createCustom(layoutPhong);
    _descriptorSetLayout[MaterialType::PHONG].push_back({"phong", descriptorSetLayout});
    _descriptorSetLayout[MaterialType::PHONG].push_back(
        {"globalPhong", _gameState->getLightManager()->getDSLGlobalPhong()});

    _descriptorSetPhong = std::make_shared<DescriptorSet>(engineState->getSettings()->getMaxFramesInFlight(),
                                                          descriptorSetLayout, engineState);
    loggerUtils->setName("Descriptor set sprite Phong", VkObjectType::VK_OBJECT_TYPE_DESCRIPTOR_SET,
                         _descriptorSetPhong->getDescriptorSets());

    setMaterial(_defaultMaterialPhong);

    // initialize Phong
    {
      auto shader = std::make_shared<Shader>(_engineState);
      shader->add("shaders/sprite/spritePhong_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/sprite/spritePhong_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

      _pipeline[MaterialType::PHONG] = std::make_shared<PipelineGraphic>(_engineState->getDevice());
      _pipeline[MaterialType::PHONG]->setDepthTest(true);
      _pipeline[MaterialType::PHONG]->setDepthWrite(true);
      _pipeline[MaterialType::PHONG]->setCullMode(VK_CULL_MODE_BACK_BIT);
      _pipeline[MaterialType::PHONG]->createCustom(
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          _descriptorSetLayout[MaterialType::PHONG],
          std::map<std::string, VkPushConstantRange>{
              {"constants", {.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = sizeof(FragmentPush)}}},
          _mesh->getBindingDescription(), _mesh->getAttributeDescriptions(), _renderPass);
      // wireframe one
      _pipelineWireframe[MaterialType::PHONG] = std::make_shared<PipelineGraphic>(_engineState->getDevice());
      _pipelineWireframe[MaterialType::PHONG]->setDepthTest(true);
      _pipelineWireframe[MaterialType::PHONG]->setDepthWrite(true);
      _pipelineWireframe[MaterialType::PHONG]->setPolygonMode(VK_POLYGON_MODE_LINE);
      _pipelineWireframe[MaterialType::PHONG]->setCullMode(VK_CULL_MODE_BACK_BIT);
      _pipelineWireframe[MaterialType::PHONG]->setAlphaBlending(false);
      _pipelineWireframe[MaterialType::PHONG]->createCustom(
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          _descriptorSetLayout[MaterialType::PHONG],
          std::map<std::string, VkPushConstantRange>{
              {"constants", {.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = sizeof(FragmentPush)}}},
          _mesh->getBindingDescription(), _mesh->getAttributeDescriptions(), _renderPass);
    }
  }

  // setup PBR
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutPBR{{.binding = 0,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 1,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 2,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 3,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 4,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 5,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 6,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 7,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 8,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 9,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 10,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr}};
    descriptorSetLayout->createCustom(layoutPBR);
    _descriptorSetLayout[MaterialType::PBR].push_back({"pbr", descriptorSetLayout});
    _descriptorSetLayout[MaterialType::PBR].push_back({"globalPBR", _gameState->getLightManager()->getDSLGlobalPBR()});

    _descriptorSetPBR = std::make_shared<DescriptorSet>(engineState->getSettings()->getMaxFramesInFlight(),
                                                        descriptorSetLayout, engineState);
    loggerUtils->setName("Descriptor set sprite PBR", VkObjectType::VK_OBJECT_TYPE_DESCRIPTOR_SET,
                         _descriptorSetPBR->getDescriptorSets());
    // phong is default, will form default descriptor only for it here
    // descriptors are formed in setMaterial

    // initialize PBR
    {
      auto shader = std::make_shared<Shader>(_engineState);
      shader->add("shaders/sprite/spritePBR_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/sprite/spritePBR_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

      _pipeline[MaterialType::PBR] = std::make_shared<PipelineGraphic>(_engineState->getDevice());
      _pipeline[MaterialType::PBR]->setDepthTest(true);
      _pipeline[MaterialType::PBR]->setDepthWrite(true);
      _pipeline[MaterialType::PBR]->setCullMode(VK_CULL_MODE_BACK_BIT);
      _pipeline[MaterialType::PBR]->createCustom(
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          _descriptorSetLayout[MaterialType::PBR],
          std::map<std::string, VkPushConstantRange>{
              {"constants", {.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = sizeof(FragmentPush)}}},
          _mesh->getBindingDescription(), _mesh->getAttributeDescriptions(), _renderPass);
      // wireframe one
      _pipelineWireframe[MaterialType::PBR] = std::make_shared<PipelineGraphic>(_engineState->getDevice());
      _pipelineWireframe[MaterialType::PBR]->setDepthTest(true);
      _pipelineWireframe[MaterialType::PBR]->setDepthWrite(true);
      _pipelineWireframe[MaterialType::PBR]->setCullMode(VK_CULL_MODE_BACK_BIT);
      _pipelineWireframe[MaterialType::PBR]->setPolygonMode(VK_POLYGON_MODE_LINE);
      _pipelineWireframe[MaterialType::PBR]->setAlphaBlending(false);
      _pipelineWireframe[MaterialType::PBR]->createCustom(
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          _descriptorSetLayout[MaterialType::PBR],
          std::map<std::string, VkPushConstantRange>{
              {"constants", {.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = sizeof(FragmentPush)}}},
          _mesh->getBindingDescription(), _mesh->getAttributeDescriptions(), _renderPass);
    }
  }

  // initialize depth directional
  {
    auto shader = std::make_shared<Shader>(_engineState);
    shader->add("shaders/sprite/spriteDepth_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/sprite/spriteDepthDirectional_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelineDirectional = std::make_shared<PipelineGraphic>(_engineState->getDevice());
    _pipelineDirectional->setDepthBias(true);
    // needed to not overwrite "depth" texture by objects that are drawn later
    _pipelineDirectional->setColorBlendOp(VK_BLEND_OP_MIN);
    _pipelineDirectional->createCustom(
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        {{"depth", _descriptorSetLayoutDepth}}, {}, _mesh->getBindingDescription(),
        _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex2D, pos)},
                                               {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex2D, color)},
                                               {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex2D, texCoord)}}),
        _renderPassDepth);
  }

  // initialize depth point
  {
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["constants"] = VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(FragmentPointLightPushDepth),
    };

    auto shader = std::make_shared<Shader>(_engineState);
    shader->add("shaders/sprite/spriteDepth_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/sprite/spriteDepthPoint_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelinePoint = std::make_shared<PipelineGraphic>(_engineState->getDevice());
    _pipelinePoint->setDepthBias(true);
    // needed to not overwrite "depth" texture by objects that are drawn later
    _pipelinePoint->setColorBlendOp(VK_BLEND_OP_MIN);
    _pipelinePoint->createCustom(
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        {{"depth", _descriptorSetLayoutDepth}}, defaultPushConstants, _mesh->getBindingDescription(),
        _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex2D, pos)},
                                               {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex2D, color)},
                                               {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex2D, texCoord)}}),
        _renderPassDepth);
  }
}

void Sprite::_updateColorDescriptor() {
  int currentFrame = _engineState->getFrameInFlight();
  auto material = std::dynamic_pointer_cast<MaterialColor>(_material);
  std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor{
      {0,
       {{.buffer = _cameraUBOFull[currentFrame]->getData(),
         .offset = 0,
         .range = _cameraUBOFull[currentFrame]->getSize()}}}};
  std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor{
      {1,
       {{.sampler = material->getBaseColor()[0]->getSampler()->getSampler(),
         .imageView = material->getBaseColor()[0]->getImageView()->getImageView(),
         .imageLayout = material->getBaseColor()[0]->getImageView()->getImage()->getImageLayout()}}}};
  _descriptorSetColor->createCustom(currentFrame, bufferInfoColor, textureInfoColor);
}

void Sprite::_updatePhongDescriptor() {
  int currentFrame = _engineState->getFrameInFlight();
  auto material = std::dynamic_pointer_cast<MaterialPhong>(_material);
  std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor{
      {0,
       {{.buffer = _cameraUBOFull[currentFrame]->getData(),
         .offset = 0,
         .range = _cameraUBOFull[currentFrame]->getSize()}}},
      {4,
       {{.buffer = material->getBufferCoefficients()[currentFrame]->getData(),
         .offset = 0,
         .range = material->getBufferCoefficients()[currentFrame]->getSize()}}}};
  std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor{
      {1,
       {{.sampler = material->getBaseColor()[0]->getSampler()->getSampler(),
         .imageView = material->getBaseColor()[0]->getImageView()->getImageView(),
         .imageLayout = material->getBaseColor()[0]->getImageView()->getImage()->getImageLayout()}}},
      {2,
       {{
           .sampler = material->getNormal()[0]->getSampler()->getSampler(),
           .imageView = material->getNormal()[0]->getImageView()->getImageView(),
           .imageLayout = material->getNormal()[0]->getImageView()->getImage()->getImageLayout(),
       }}},
      {3,
       {{.sampler = material->getSpecular()[0]->getSampler()->getSampler(),
         .imageView = material->getSpecular()[0]->getImageView()->getImageView(),
         .imageLayout = material->getSpecular()[0]->getImageView()->getImage()->getImageLayout()}}}};
  _descriptorSetPhong->createCustom(currentFrame, bufferInfoColor, textureInfoColor);
}

void Sprite::_updatePBRDescriptor() {
  int currentFrame = _engineState->getFrameInFlight();
  auto material = std::dynamic_pointer_cast<MaterialPBR>(_material);
  std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor{
      {0,
       {{.buffer = _cameraUBOFull[currentFrame]->getData(),
         .offset = 0,
         .range = _cameraUBOFull[currentFrame]->getSize()}}},
      {10,
       {{.buffer = material->getBufferCoefficients()[currentFrame]->getData(),
         .offset = 0,
         .range = material->getBufferCoefficients()[currentFrame]->getSize()}}}};
  std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor{
      {1,
       {{.sampler = material->getBaseColor()[0]->getSampler()->getSampler(),
         .imageView = material->getBaseColor()[0]->getImageView()->getImageView(),
         .imageLayout = material->getBaseColor()[0]->getImageView()->getImage()->getImageLayout()}}},
      {2,
       {{.sampler = material->getNormal()[0]->getSampler()->getSampler(),
         .imageView = material->getNormal()[0]->getImageView()->getImageView(),
         .imageLayout = material->getNormal()[0]->getImageView()->getImage()->getImageLayout()}}},
      {3,
       {{.sampler = material->getMetallic()[0]->getSampler()->getSampler(),
         .imageView = material->getMetallic()[0]->getImageView()->getImageView(),
         .imageLayout = material->getMetallic()[0]->getImageView()->getImage()->getImageLayout()}}},
      {4,
       {{.sampler = material->getRoughness()[0]->getSampler()->getSampler(),
         .imageView = material->getRoughness()[0]->getImageView()->getImageView(),
         .imageLayout = material->getRoughness()[0]->getImageView()->getImage()->getImageLayout()}}},
      {5,
       {{.sampler = material->getOccluded()[0]->getSampler()->getSampler(),
         .imageView = material->getOccluded()[0]->getImageView()->getImageView(),
         .imageLayout = material->getOccluded()[0]->getImageView()->getImage()->getImageLayout()}}},
      {6,
       {{.sampler = material->getEmissive()[0]->getSampler()->getSampler(),
         .imageView = material->getEmissive()[0]->getImageView()->getImageView(),
         .imageLayout = material->getEmissive()[0]->getImageView()->getImage()->getImageLayout()}}},
      {7,
       {{.sampler = material->getDiffuseIBL()->getSampler()->getSampler(),
         .imageView = material->getDiffuseIBL()->getImageView()->getImageView(),
         .imageLayout = material->getDiffuseIBL()->getImageView()->getImage()->getImageLayout()}}},
      {8,
       {{.sampler = material->getSpecularIBL()->getSampler()->getSampler(),
         .imageView = material->getSpecularIBL()->getImageView()->getImageView(),
         .imageLayout = material->getSpecularIBL()->getImageView()->getImage()->getImageLayout()}}},
      {9,
       {{.sampler = material->getSpecularBRDF()->getSampler()->getSampler(),
         .imageView = material->getSpecularBRDF()->getImageView()->getImageView(),
         .imageLayout = material->getSpecularBRDF()->getImageView()->getImage()->getImageLayout()}}}};

  _descriptorSetPBR->createCustom(currentFrame, bufferInfoColor, textureInfoColor);
}

void Sprite::setMaterial(std::shared_ptr<MaterialPBR> material) {
  if (_material) _material->unregisterUpdate(_descriptorSetPBR);
  material->registerUpdate(_descriptorSetPBR, {{MaterialTexture::COLOR, 1},
                                               {MaterialTexture::NORMAL, 2},
                                               {MaterialTexture::METALLIC, 3},
                                               {MaterialTexture::ROUGHNESS, 4},
                                               {MaterialTexture::OCCLUSION, 5},
                                               {MaterialTexture::EMISSIVE, 6},
                                               {MaterialTexture::IBL_DIFFUSE, 7},
                                               {MaterialTexture::IBL_SPECULAR, 8},
                                               {MaterialTexture::BRDF_SPECULAR, 9}});
  for (int d = 0; d < _engineState->getSettings()->getMaxDirectionalLights(); d++) {
    if (_material) _material->unregisterUpdate(_descriptorSetCameraDepth[d][0]);
    material->registerUpdate(_descriptorSetCameraDepth[d][0], {{MaterialTexture::COLOR, 1}});
  }
  for (int p = 0; p < _engineState->getSettings()->getMaxPointLights(); p++) {
    for (int f = 0; f < 6; f++) {
      if (_material)
        _material->unregisterUpdate(
            _descriptorSetCameraDepth[_engineState->getSettings()->getMaxDirectionalLights() + p][f]);
      material->registerUpdate(_descriptorSetCameraDepth[_engineState->getSettings()->getMaxDirectionalLights() + p][f],
                               {{MaterialTexture::COLOR, 1}});
    }
  }
  _materialType = MaterialType::PBR;
  _material = material;
  for (int i = 0; i < _changedMaterialRender.size(); i++) {
    _changedMaterialRender[i] = true;
    _changedMaterialShadow[i] = true;
  }
}

void Sprite::setMaterial(std::shared_ptr<MaterialPhong> material) {
  if (_material) _material->unregisterUpdate(_descriptorSetPhong);
  material->registerUpdate(_descriptorSetPhong,
                           {{MaterialTexture::COLOR, 1}, {MaterialTexture::NORMAL, 2}, {MaterialTexture::SPECULAR, 3}});
  for (int d = 0; d < _engineState->getSettings()->getMaxDirectionalLights(); d++) {
    if (_material) _material->unregisterUpdate(_descriptorSetCameraDepth[d][0]);
    material->registerUpdate(_descriptorSetCameraDepth[d][0], {{MaterialTexture::COLOR, 1}});
  }
  for (int p = 0; p < _engineState->getSettings()->getMaxPointLights(); p++) {
    for (int f = 0; f < 6; f++) {
      if (_material)
        _material->unregisterUpdate(
            _descriptorSetCameraDepth[_engineState->getSettings()->getMaxDirectionalLights() + p][f]);
      material->registerUpdate(_descriptorSetCameraDepth[_engineState->getSettings()->getMaxDirectionalLights() + p][f],
                               {{MaterialTexture::COLOR, 1}});
    }
  }
  _materialType = MaterialType::PHONG;
  _material = material;
  for (int i = 0; i < _changedMaterialRender.size(); i++) {
    _changedMaterialRender[i] = true;
    _changedMaterialShadow[i] = true;
  }
}

void Sprite::setMaterial(std::shared_ptr<MaterialColor> material) {
  if (_material) _material->unregisterUpdate(_descriptorSetColor);
  material->registerUpdate(_descriptorSetColor, {{MaterialTexture::COLOR, 1}});
  for (int d = 0; d < _engineState->getSettings()->getMaxDirectionalLights(); d++) {
    if (_material) _material->unregisterUpdate(_descriptorSetCameraDepth[d][0]);
    material->registerUpdate(_descriptorSetCameraDepth[d][0], {{MaterialTexture::COLOR, 1}});
  }
  for (int p = 0; p < _engineState->getSettings()->getMaxPointLights(); p++) {
    for (int f = 0; f < 6; f++) {
      if (_material)
        _material->unregisterUpdate(
            _descriptorSetCameraDepth[_engineState->getSettings()->getMaxDirectionalLights() + p][f]);
      material->registerUpdate(_descriptorSetCameraDepth[_engineState->getSettings()->getMaxDirectionalLights() + p][f],
                               {{MaterialTexture::COLOR, 1}});
    }
  }

  _materialType = MaterialType::COLOR;
  _material = material;
  for (int i = 0; i < _changedMaterialRender.size(); i++) {
    _changedMaterialRender[i] = true;
    _changedMaterialShadow[i] = true;
  }
}

MaterialType Sprite::getMaterialType() { return _materialType; }

void Sprite::enableDepth(bool enable) { _enableDepth = enable; }

void Sprite::enableHUD(bool enable) { _enableHUD = enable; }

bool Sprite::isDepthEnabled() { return _enableDepth; }

void Sprite::enableShadow(bool enable) { _enableShadow = enable; }

void Sprite::enableLighting(bool enable) { _enableLighting = enable; }

void Sprite::setDrawType(DrawType drawType) { _drawType = drawType; }

DrawType Sprite::getDrawType() { return _drawType; }

void Sprite::draw(std::shared_ptr<CommandBuffer> commandBuffer) {
  int currentFrame = _engineState->getFrameInFlight();
  if (_changedMaterialRender[currentFrame]) {
    switch (_materialType) {
      case MaterialType::COLOR:
        _updateColorDescriptor();
        break;
      case MaterialType::PHONG:
        _updatePhongDescriptor();
        break;
      case MaterialType::PBR:
        _updatePBRDescriptor();
        break;
    }
    _changedMaterialRender[currentFrame] = false;
  }

  auto pipeline = _pipeline[_materialType];
  if (_drawType == DrawType::WIREFRAME) pipeline = _pipelineWireframe[_materialType];
  if (_drawType == DrawType::NORMAL) pipeline = _pipelineNormal;
  if (_drawType == DrawType::TANGENT) pipeline = _pipelineTangent;

  auto resolution = _engineState->getSettings()->getResolution();
  VkViewport viewport{.x = 0.0f,
                      .y = static_cast<float>(std::get<1>(resolution)),
                      .width = static_cast<float>(std::get<0>(resolution)),
                      .height = static_cast<float>(-std::get<1>(resolution)),
                      .minDepth = 0.0f,
                      .maxDepth = 1.0f};
  vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);
  VkRect2D scissor{.offset = {0, 0}, .extent = VkExtent2D(std::get<0>(resolution), std::get<1>(resolution))};
  vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  if (pipeline->getPushConstants().find("constants") != pipeline->getPushConstants().end()) {
    FragmentPush pushConstants{.enableShadow = _enableShadow,
                               .enableLighting = _enableLighting,
                               .cameraPosition = _gameState->getCameraManager()->getCurrentCamera()->getEye()};
    auto info = pipeline->getPushConstants()["constants"];
    vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(), info.stageFlags,
                       info.offset, info.size, &pushConstants);
  }

  BufferMVP cameraMVP{.model = getModel(), .view = glm::mat4(1.f), .projection = glm::mat4(1.f)};
  if (_enableHUD == false) {
    cameraMVP.view = _gameState->getCameraManager()->getCurrentCamera()->getView();
    cameraMVP.projection = _gameState->getCameraManager()->getCurrentCamera()->getProjection();
  }

  _cameraUBOFull[currentFrame]->setData(&cameraMVP);

  VkBuffer vertexBuffers[] = {_mesh->getVertexBuffer()->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame], _mesh->getIndexBuffer()->getBuffer()->getData(),
                       0, VK_INDEX_TYPE_UINT32);

  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->getPipeline());

  // color
  auto pipelineLayout = pipeline->getDescriptorSetLayout();
  auto colorLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                  [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                    return info.first == std::string("color");
                                  });
  if (colorLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->getPipelineLayout(), 0, 1,
                            &_descriptorSetColor->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  // Phong
  auto phongLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                  [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                    return info.first == std::string("phong");
                                  });
  if (phongLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->getPipelineLayout(), 0, 1,
                            &_descriptorSetPhong->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  // global Phong
  auto globalLayoutPhong = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                          return info.first == std::string("globalPhong");
                                        });
  if (globalLayoutPhong != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(),
        1, 1, &_gameState->getLightManager()->getDSGlobalPhong()->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  // PBR
  auto pbrLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                  return info.first == std::string("pbr");
                                });
  if (pbrLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->getPipelineLayout(), 0, 1, &_descriptorSetPBR->getDescriptorSets()[currentFrame],
                            0, nullptr);
  }

  // global PBR
  auto globalLayoutPBR = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                        return info.first == std::string("globalPBR");
                                      });
  if (globalLayoutPBR != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(),
        1, 1, &_gameState->getLightManager()->getDSGlobalPBR()->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  // normals and tangents
  auto normalTangentLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                          [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                            return info.first == std::string("normal");
                                          });
  if (normalTangentLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->getPipelineLayout(), 0, 1,
                            &_descriptorSetNormalsMesh->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_mesh->getIndexData().size()),
                   1, 0, 0, 0);
}

void Sprite::drawShadow(LightType lightType, int lightIndex, int face, std::shared_ptr<CommandBuffer> commandBuffer) {
  if (_enableDepth == false) return;

  int currentFrame = _engineState->getFrameInFlight();
  {
    // we have to update only once during the first drawShadow call
    // (for point lights this function is called multiple times in different threads)
    std::unique_lock<std::mutex> lock(_updateShadow);
    if (_changedMaterialShadow[currentFrame]) {
      switch (_materialType) {
        case MaterialType::COLOR:
          _updateShadowDescriptor(std::dynamic_pointer_cast<MaterialColor>(_material));
          break;
        case MaterialType::PHONG:
          _updateShadowDescriptor(std::dynamic_pointer_cast<MaterialPhong>(_material));
          break;
        case MaterialType::PBR:
          _updateShadowDescriptor(std::dynamic_pointer_cast<MaterialPBR>(_material));
          break;
      }
      _changedMaterialShadow[currentFrame] = false;
    }
  }

  auto pipeline = _pipelineDirectional;
  if (lightType == LightType::POINT) pipeline = _pipelinePoint;

  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->getPipeline());
  std::tuple<int, int> resolution = _engineState->getSettings()->getShadowMapResolution();
  // Cube Maps have been specified to follow the RenderMan specification (for whatever reason),
  // and RenderMan assumes the images' origin being in the upper left so we don't need to swap anything
  // if we swap, we need to change shader as well, so swap there. But we can't do it there because we sample from
  // cubemap and we can't just (1 - y)
  VkViewport viewport{};
  if (lightType == LightType::DIRECTIONAL) {
    viewport = {.x = 0.0f,
                .y = static_cast<float>(std::get<1>(resolution)),
                .width = static_cast<float>(std::get<0>(resolution)),
                .height = static_cast<float>(-std::get<1>(resolution))};
  } else if (lightType == LightType::POINT) {
    viewport = {.x = 0.0f,
                .y = 0.f,
                .width = static_cast<float>(std::get<0>(resolution)),
                .height = static_cast<float>(std::get<1>(resolution))};
  }
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{.offset = {0, 0}, .extent = VkExtent2D(std::get<0>(resolution), std::get<1>(resolution))};
  vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  if (pipeline->getPushConstants().find("constants") != pipeline->getPushConstants().end()) {
    if (lightType == LightType::POINT) {
      FragmentPointLightPushDepth pushConstants;
      pushConstants.lightPosition =
          _gameState->getLightManager()->getPointLights()[lightIndex]->getCamera()->getPosition();
      // light camera
      pushConstants.far = _gameState->getLightManager()->getPointLights()[lightIndex]->getCamera()->getFar();
      auto info = pipeline->getPushConstants()["constants"];
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         info.stageFlags, info.offset, info.size, &pushConstants);
    }
  }

  glm::mat4 view(1.f);
  glm::mat4 projection(1.f);
  int lightIndexTotal = lightIndex;
  if (lightType == LightType::DIRECTIONAL) {
    view = _gameState->getLightManager()->getDirectionalLights()[lightIndex]->getCamera()->getView();
    projection = _gameState->getLightManager()->getDirectionalLights()[lightIndex]->getCamera()->getProjection();
  }
  if (lightType == LightType::POINT) {
    lightIndexTotal += _engineState->getSettings()->getMaxDirectionalLights();
    view = _gameState->getLightManager()->getPointLights()[lightIndex]->getCamera()->getView(face);
    projection = _gameState->getLightManager()->getPointLights()[lightIndex]->getCamera()->getProjection();
  }

  BufferMVP cameraMVP{.model = getModel(), .view = view, .projection = projection};
  _cameraUBODepth[lightIndexTotal][face][currentFrame]->setData(&cameraMVP);

  VkBuffer vertexBuffers[] = {_mesh->getVertexBuffer()->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame], _mesh->getIndexBuffer()->getBuffer()->getData(),
                       0, VK_INDEX_TYPE_UINT32);

  auto pipelineLayout = pipeline->getDescriptorSetLayout();
  auto depthLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                  [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                    return info.first == std::string("depth");
                                  });
  if (depthLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(),
        0, 1, &_descriptorSetCameraDepth[lightIndexTotal][face]->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_mesh->getIndexData().size()),
                   1, 0, 0, 0);
}