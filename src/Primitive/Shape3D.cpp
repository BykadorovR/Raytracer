#include "Shape3D.h"
#undef far

Shape3D::Shape3D(ShapeType shapeType,
                 VkCullModeFlags cullMode,
                 std::shared_ptr<LightManager> lightManager,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<ResourceManager> resourceManager,
                 std::shared_ptr<State> state) {
  setName("Shape3D");
  _shapeType = shapeType;
  _state = state;
  _lightManager = lightManager;
  _cullMode = cullMode;

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
  _renderPass = std::make_shared<RenderPass>(_state->getSettings(), _state->getDevice());
  _renderPass->initializeGraphic();
  _renderPassDepth = std::make_shared<RenderPass>(_state->getSettings(), _state->getDevice());
  _renderPassDepth->initializeLightDepth();

  _uniformBufferCamera = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(),
                                                         sizeof(BufferMVP), state);
  // setup normals
  {
    _descriptorSetLayoutNormalsMesh = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutNormalsMesh(2);
    layoutNormalsMesh[0].binding = 0;
    layoutNormalsMesh[0].descriptorCount = 1;
    layoutNormalsMesh[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutNormalsMesh[0].pImmutableSamplers = nullptr;
    layoutNormalsMesh[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layoutNormalsMesh[1].binding = 1;
    layoutNormalsMesh[1].descriptorCount = 1;
    layoutNormalsMesh[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutNormalsMesh[1].pImmutableSamplers = nullptr;
    layoutNormalsMesh[1].stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT;
    _descriptorSetLayoutNormalsMesh->createCustom(layoutNormalsMesh);

    _descriptorSetNormalsMesh = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                                _descriptorSetLayoutNormalsMesh,
                                                                state->getDescriptorPool(), state->getDevice());
    for (int i = 0; i < state->getSettings()->getMaxFramesInFlight(); i++) {
      std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoNormalsMesh;
      std::vector<VkDescriptorBufferInfo> bufferInfoVertex(1);
      // write to binding = 0 for vertex shader
      bufferInfoVertex[0].buffer = _uniformBufferCamera->getBuffer()[i]->getData();
      bufferInfoVertex[0].offset = 0;
      bufferInfoVertex[0].range = sizeof(BufferMVP);
      bufferInfoNormalsMesh[0] = bufferInfoVertex;
      // write for binding = 1 for geometry shader
      std::vector<VkDescriptorBufferInfo> bufferInfoGeometry(1);
      bufferInfoGeometry[0].buffer = _uniformBufferCamera->getBuffer()[i]->getData();
      bufferInfoGeometry[0].offset = 0;
      bufferInfoGeometry[0].range = sizeof(BufferMVP);
      bufferInfoNormalsMesh[1] = bufferInfoGeometry;
      _descriptorSetNormalsMesh->createCustom(i, bufferInfoNormalsMesh, {});
    }

    // initialize Normal (per vertex)
    {
      auto shader = std::make_shared<Shader>(state);
      shader->add(_shadersNormalsMesh[_shapeType][0], VK_SHADER_STAGE_VERTEX_BIT);
      shader->add(_shadersNormalsMesh[_shapeType][1], VK_SHADER_STAGE_FRAGMENT_BIT);
      shader->add(_shadersNormalsMesh[_shapeType][2], VK_SHADER_STAGE_GEOMETRY_BIT);

      _pipelineNormalMesh = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      _pipelineNormalMesh->createGraphic3D(
          cullMode, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
          std::vector{std::pair{std::string("normal"), _descriptorSetLayoutNormalsMesh}}, {},
          _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, normal)},
                                                 {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)}}),
          _renderPass);
    }
    // initialize Tangent (per vertex)
    {
      auto shader = std::make_shared<Shader>(state);
      shader->add(_shadersTangentMesh[_shapeType][0], VK_SHADER_STAGE_VERTEX_BIT);
      shader->add(_shadersTangentMesh[_shapeType][1], VK_SHADER_STAGE_FRAGMENT_BIT);
      shader->add(_shadersTangentMesh[_shapeType][2], VK_SHADER_STAGE_GEOMETRY_BIT);

      _pipelineTangentMesh = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      _pipelineTangentMesh->createGraphic3D(
          cullMode, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
          std::vector{std::pair{std::string("normal"), _descriptorSetLayoutNormalsMesh}}, {},
          _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)},
                                                 {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex3D, tangent)}}),
          _renderPass);
    }
  }
  // setup color
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutColor(2);
    layoutColor[0].binding = 0;
    layoutColor[0].descriptorCount = 1;
    layoutColor[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutColor[0].pImmutableSamplers = nullptr;
    layoutColor[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layoutColor[1].binding = 1;
    layoutColor[1].descriptorCount = 1;
    layoutColor[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutColor[1].pImmutableSamplers = nullptr;
    layoutColor[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayout->createCustom(layoutColor);
    _descriptorSetLayout[MaterialType::COLOR].push_back({"color", descriptorSetLayout});

    _descriptorSetColor = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                          descriptorSetLayout, state->getDescriptorPool(),
                                                          state->getDevice());
    _updateColorDescriptor(_defaultMaterialColor);
    // initialize color
    {
      auto shader = std::make_shared<Shader>(_state);
      shader->add(_shadersColor[_shapeType][MaterialType::COLOR][0], VK_SHADER_STAGE_VERTEX_BIT);
      shader->add(_shadersColor[_shapeType][MaterialType::COLOR][1], VK_SHADER_STAGE_FRAGMENT_BIT);
      _pipeline[_shapeType][MaterialType::COLOR] = std::make_shared<Pipeline>(_state->getSettings(),
                                                                              _state->getDevice());
      std::vector<std::tuple<VkFormat, uint32_t>> attributes;
      if (_shapeType == ShapeType::CUBE) {
        attributes = {{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, normal)},
                      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)}};
      } else if (_shapeType == ShapeType::SPHERE) {
        attributes = {{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, normal)},
                      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)},
                      {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}};
      }
      _pipeline[_shapeType][MaterialType::COLOR]->createGraphic3D(
          _cullMode, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          _descriptorSetLayout[MaterialType::COLOR], {}, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions(attributes), _renderPass);
      _pipelineWireframe[_shapeType][MaterialType::COLOR] = std::make_shared<Pipeline>(_state->getSettings(),
                                                                                       _state->getDevice());
      _pipelineWireframe[_shapeType][MaterialType::COLOR]->createGraphic3D(
          _cullMode, VK_POLYGON_MODE_LINE,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          _descriptorSetLayout[MaterialType::COLOR], {}, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions(attributes), _renderPass);
    }
  }

  // setup Phong
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutPhong(5);
    layoutPhong[0].binding = 0;
    layoutPhong[0].descriptorCount = 1;
    layoutPhong[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPhong[0].pImmutableSamplers = nullptr;
    layoutPhong[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layoutPhong[1].binding = 1;
    layoutPhong[1].descriptorCount = 1;
    layoutPhong[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPhong[1].pImmutableSamplers = nullptr;
    layoutPhong[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPhong[2].binding = 2;
    layoutPhong[2].descriptorCount = 1;
    layoutPhong[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPhong[2].pImmutableSamplers = nullptr;
    layoutPhong[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPhong[3].binding = 3;
    layoutPhong[3].descriptorCount = 1;
    layoutPhong[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPhong[3].pImmutableSamplers = nullptr;
    layoutPhong[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPhong[4].binding = 4;
    layoutPhong[4].descriptorCount = 1;
    layoutPhong[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPhong[4].pImmutableSamplers = nullptr;
    layoutPhong[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayout->createCustom(layoutPhong);
    _descriptorSetLayout[MaterialType::PHONG].push_back({"phong", descriptorSetLayout});
    _descriptorSetLayout[MaterialType::PHONG].push_back({"globalPhong", lightManager->getDSLGlobalPhong()});

    _descriptorSetPhong = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                          descriptorSetLayout, state->getDescriptorPool(),
                                                          state->getDevice());
    // update descriptor set in setMaterial

    // initialize Phong
    {
      auto shader = std::make_shared<Shader>(state);
      shader->add(_shadersColor[_shapeType][MaterialType::PHONG][0], VK_SHADER_STAGE_VERTEX_BIT);
      shader->add(_shadersColor[_shapeType][MaterialType::PHONG][1], VK_SHADER_STAGE_FRAGMENT_BIT);

      _pipeline[_shapeType][MaterialType::PHONG] = std::make_shared<Pipeline>(_state->getSettings(),
                                                                              _state->getDevice());

      std::vector<std::tuple<VkFormat, uint32_t>> attributes;
      if (_shapeType == ShapeType::CUBE) {
        attributes = {{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, normal)},
                      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)},
                      {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex3D, tangent)}};
      } else if (_shapeType == ShapeType::SPHERE) {
        attributes = {{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, normal)},
                      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)},
                      {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)},
                      {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex3D, tangent)}};
      }
      _pipeline[_shapeType][MaterialType::PHONG]->createGraphic3D(
          cullMode, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          _descriptorSetLayout[MaterialType::PHONG],
          std::map<std::string, VkPushConstantRange>{{std::string("constants"), LightPush::getPushConstant()}},
          _mesh->getBindingDescription(), _mesh->Mesh::getAttributeDescriptions(attributes), _renderPass);
      // wireframe one
      _pipelineWireframe[_shapeType][MaterialType::PHONG] = std::make_shared<Pipeline>(_state->getSettings(),
                                                                                       _state->getDevice());
      _pipelineWireframe[_shapeType][MaterialType::PHONG]->createGraphic3D(
          cullMode, VK_POLYGON_MODE_LINE,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          _descriptorSetLayout[MaterialType::PHONG],
          std::map<std::string, VkPushConstantRange>{{std::string("constants"), LightPush::getPushConstant()}},
          _mesh->getBindingDescription(), _mesh->Mesh::getAttributeDescriptions(attributes), _renderPass);
    }
  }

  // setup PBR
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutPBR(12);
    layoutPBR[0].binding = 0;
    layoutPBR[0].descriptorCount = 1;
    layoutPBR[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPBR[0].pImmutableSamplers = nullptr;
    layoutPBR[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layoutPBR[1].binding = 1;
    layoutPBR[1].descriptorCount = 1;
    layoutPBR[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[1].pImmutableSamplers = nullptr;
    layoutPBR[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[2].binding = 2;
    layoutPBR[2].descriptorCount = 1;
    layoutPBR[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[2].pImmutableSamplers = nullptr;
    layoutPBR[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[3].binding = 3;
    layoutPBR[3].descriptorCount = 1;
    layoutPBR[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[3].pImmutableSamplers = nullptr;
    layoutPBR[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[4].binding = 4;
    layoutPBR[4].descriptorCount = 1;
    layoutPBR[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[4].pImmutableSamplers = nullptr;
    layoutPBR[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[5].binding = 5;
    layoutPBR[5].descriptorCount = 1;
    layoutPBR[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[5].pImmutableSamplers = nullptr;
    layoutPBR[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[6].binding = 6;
    layoutPBR[6].descriptorCount = 1;
    layoutPBR[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[6].pImmutableSamplers = nullptr;
    layoutPBR[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[7].binding = 7;
    layoutPBR[7].descriptorCount = 1;
    layoutPBR[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[7].pImmutableSamplers = nullptr;
    layoutPBR[7].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[8].binding = 8;
    layoutPBR[8].descriptorCount = 1;
    layoutPBR[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[8].pImmutableSamplers = nullptr;
    layoutPBR[8].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[9].binding = 9;
    layoutPBR[9].descriptorCount = 1;
    layoutPBR[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[9].pImmutableSamplers = nullptr;
    layoutPBR[9].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[10].binding = 10;
    layoutPBR[10].descriptorCount = 1;
    layoutPBR[10].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPBR[10].pImmutableSamplers = nullptr;
    layoutPBR[10].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[11].binding = 11;
    layoutPBR[11].descriptorCount = 1;
    layoutPBR[11].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPBR[11].pImmutableSamplers = nullptr;
    layoutPBR[11].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayout->createCustom(layoutPBR);
    _descriptorSetLayout[MaterialType::PBR].push_back({"pbr", descriptorSetLayout});
    _descriptorSetLayout[MaterialType::PBR].push_back({"globalPBR", lightManager->getDSLGlobalPBR()});

    _descriptorSetPBR = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                        descriptorSetLayout, state->getDescriptorPool(),
                                                        state->getDevice());
    // update descriptor set in setMaterial

    // initialize PBR
    {
      auto shader = std::make_shared<Shader>(_state);
      shader->add(_shadersColor[_shapeType][MaterialType::PBR][0], VK_SHADER_STAGE_VERTEX_BIT);
      shader->add(_shadersColor[_shapeType][MaterialType::PBR][1], VK_SHADER_STAGE_FRAGMENT_BIT);

      _pipeline[_shapeType][MaterialType::PBR] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      std::vector<std::tuple<VkFormat, uint32_t>> attributes;
      if (_shapeType == ShapeType::CUBE) {
        attributes = {{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, normal)},
                      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)},
                      {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex3D, tangent)}};
      } else if (_shapeType == ShapeType::SPHERE) {
        attributes = {{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, normal)},
                      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)},
                      {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)},
                      {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex3D, tangent)}};
      }
      _pipeline[_shapeType][MaterialType::PBR]->createGraphic3D(
          cullMode, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          _descriptorSetLayout[MaterialType::PBR],
          std::map<std::string, VkPushConstantRange>{{std::string("constants"), LightPush::getPushConstant()}},
          _mesh->getBindingDescription(), _mesh->Mesh::getAttributeDescriptions(attributes), _renderPass);
      // wireframe one
      _pipelineWireframe[_shapeType][MaterialType::PBR] = std::make_shared<Pipeline>(_state->getSettings(),
                                                                                     _state->getDevice());
      _pipelineWireframe[_shapeType][MaterialType::PBR]->createGraphic3D(
          cullMode, VK_POLYGON_MODE_LINE,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          _descriptorSetLayout[MaterialType::PBR],
          std::map<std::string, VkPushConstantRange>{{std::string("constants"), LightPush::getPushConstant()}},
          _mesh->getBindingDescription(), _mesh->Mesh::getAttributeDescriptions(attributes), _renderPass);
    }
  }

  // shadows
  auto layoutCamera = std::make_shared<DescriptorSetLayout>(state->getDevice());
  layoutCamera->createUniformBuffer();

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

  // initialize shadows directional
  {
    auto shader = std::make_shared<Shader>(_state);
    shader->add(_shadersLight[_shapeType][0], VK_SHADER_STAGE_VERTEX_BIT);
    _pipelineDirectional = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineDirectional->createGraphic3DShadow(
        VK_CULL_MODE_NONE, {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT)}, {{"depth", layoutCamera}}, {},
        _mesh->getBindingDescription(),
        _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)}}),
        _renderPassDepth);
  }

  // initialize shadows point
  {
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["constants"] = DepthConstants::getPushConstant(0);

    auto shader = std::make_shared<Shader>(_state);
    shader->add(_shadersLight[_shapeType][0], VK_SHADER_STAGE_VERTEX_BIT);
    shader->add(_shadersLight[_shapeType][1], VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelinePoint = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelinePoint->createGraphic3DShadow(
        VK_CULL_MODE_NONE,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        {{"depth", layoutCamera}}, defaultPushConstants, _mesh->getBindingDescription(),
        _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)}}),
        _renderPassDepth);
  }
}

void Shape3D::_updateColorDescriptor(std::shared_ptr<MaterialColor> material) {
  std::vector<VkDescriptorImageInfo> colorImageInfo(_state->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
    std::vector<VkDescriptorBufferInfo> bufferInfoCamera(1);
    // write to binding = 0 for vertex shader
    bufferInfoCamera[0].buffer = _uniformBufferCamera->getBuffer()[i]->getData();
    bufferInfoCamera[0].offset = 0;
    bufferInfoCamera[0].range = sizeof(BufferMVP);
    bufferInfoColor[0] = bufferInfoCamera;

    // write for binding = 1 for textures
    std::vector<VkDescriptorImageInfo> bufferInfoTexture(1);
    bufferInfoTexture[0].imageLayout = material->getBaseColor()[0]->getImageView()->getImage()->getImageLayout();
    bufferInfoTexture[0].imageView = material->getBaseColor()[0]->getImageView()->getImageView();
    bufferInfoTexture[0].sampler = material->getBaseColor()[0]->getSampler()->getSampler();
    textureInfoColor[1] = bufferInfoTexture;
    _descriptorSetColor->createCustom(i, bufferInfoColor, textureInfoColor);
  }
  material->unregisterUpdate(_descriptorSetColor);
  material->registerUpdate(_descriptorSetColor, {{MaterialTexture::COLOR, 1}});
}

void Shape3D::_updatePhongDescriptor(std::shared_ptr<MaterialPhong> material) {
  std::vector<VkDescriptorImageInfo> colorImageInfo(_state->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
    std::vector<VkDescriptorBufferInfo> bufferInfoCamera(1);
    // write to binding = 0 for vertex shader
    bufferInfoCamera[0].buffer = _uniformBufferCamera->getBuffer()[i]->getData();
    bufferInfoCamera[0].offset = 0;
    bufferInfoCamera[0].range = sizeof(BufferMVP);
    bufferInfoColor[0] = bufferInfoCamera;

    // write for binding = 1 for textures
    std::vector<VkDescriptorImageInfo> bufferInfoBaseColor(1);
    bufferInfoBaseColor[0].imageLayout = material->getBaseColor()[0]->getImageView()->getImage()->getImageLayout();
    bufferInfoBaseColor[0].imageView = material->getBaseColor()[0]->getImageView()->getImageView();
    bufferInfoBaseColor[0].sampler = material->getBaseColor()[0]->getSampler()->getSampler();
    textureInfoColor[1] = bufferInfoBaseColor;

    std::vector<VkDescriptorImageInfo> bufferInfoNormal(1);
    bufferInfoNormal[0].imageLayout = material->getNormal()[0]->getImageView()->getImage()->getImageLayout();
    bufferInfoNormal[0].imageView = material->getNormal()[0]->getImageView()->getImageView();
    bufferInfoNormal[0].sampler = material->getNormal()[0]->getSampler()->getSampler();
    textureInfoColor[2] = bufferInfoNormal;

    std::vector<VkDescriptorImageInfo> bufferInfoSpecular(1);
    bufferInfoSpecular[0].imageLayout = material->getSpecular()[0]->getImageView()->getImage()->getImageLayout();
    bufferInfoSpecular[0].imageView = material->getSpecular()[0]->getImageView()->getImageView();
    bufferInfoSpecular[0].sampler = material->getSpecular()[0]->getSampler()->getSampler();
    textureInfoColor[3] = bufferInfoSpecular;

    std::vector<VkDescriptorBufferInfo> bufferInfoCoefficients(1);
    // write to binding = 0 for vertex shader
    bufferInfoCoefficients[0].buffer = material->getBufferCoefficients()->getBuffer()[i]->getData();
    bufferInfoCoefficients[0].offset = 0;
    bufferInfoCoefficients[0].range = material->getBufferCoefficients()->getBuffer()[i]->getSize();
    bufferInfoColor[4] = bufferInfoCoefficients;
    _descriptorSetPhong->createCustom(i, bufferInfoColor, textureInfoColor);
  }
  material->unregisterUpdate(_descriptorSetPhong);
  material->registerUpdate(_descriptorSetPhong,
                           {{MaterialTexture::COLOR, 1}, {MaterialTexture::NORMAL, 2}, {MaterialTexture::SPECULAR, 3}});
}

void Shape3D::_updatePBRDescriptor(std::shared_ptr<MaterialPBR> material) {
  std::vector<VkDescriptorImageInfo> colorImageInfo(_state->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
    std::vector<VkDescriptorBufferInfo> bufferInfoCamera(1);
    // write to binding = 0 for vertex shader
    bufferInfoCamera[0].buffer = _uniformBufferCamera->getBuffer()[i]->getData();
    bufferInfoCamera[0].offset = 0;
    bufferInfoCamera[0].range = sizeof(BufferMVP);
    bufferInfoColor[0] = bufferInfoCamera;

    // write for binding = 1 for textures
    std::vector<VkDescriptorImageInfo> bufferInfoBaseColor(1);
    bufferInfoBaseColor[0].imageLayout = material->getBaseColor()[0]->getImageView()->getImage()->getImageLayout();
    bufferInfoBaseColor[0].imageView = material->getBaseColor()[0]->getImageView()->getImageView();
    bufferInfoBaseColor[0].sampler = material->getBaseColor()[0]->getSampler()->getSampler();
    textureInfoColor[1] = bufferInfoBaseColor;

    std::vector<VkDescriptorImageInfo> bufferInfoNormal(1);
    bufferInfoNormal[0].imageLayout = material->getNormal()[0]->getImageView()->getImage()->getImageLayout();
    bufferInfoNormal[0].imageView = material->getNormal()[0]->getImageView()->getImageView();
    bufferInfoNormal[0].sampler = material->getNormal()[0]->getSampler()->getSampler();
    textureInfoColor[2] = bufferInfoNormal;

    std::vector<VkDescriptorImageInfo> bufferInfoMetallic(1);
    bufferInfoMetallic[0].imageLayout = material->getMetallic()[0]->getImageView()->getImage()->getImageLayout();
    bufferInfoMetallic[0].imageView = material->getMetallic()[0]->getImageView()->getImageView();
    bufferInfoMetallic[0].sampler = material->getMetallic()[0]->getSampler()->getSampler();
    textureInfoColor[3] = bufferInfoMetallic;

    std::vector<VkDescriptorImageInfo> bufferInfoRoughness(1);
    bufferInfoRoughness[0].imageLayout = material->getRoughness()[0]->getImageView()->getImage()->getImageLayout();
    bufferInfoRoughness[0].imageView = material->getRoughness()[0]->getImageView()->getImageView();
    bufferInfoRoughness[0].sampler = material->getRoughness()[0]->getSampler()->getSampler();
    textureInfoColor[4] = bufferInfoRoughness;

    std::vector<VkDescriptorImageInfo> bufferInfoOcclusion(1);
    bufferInfoOcclusion[0].imageLayout = material->getOccluded()[0]->getImageView()->getImage()->getImageLayout();
    bufferInfoOcclusion[0].imageView = material->getOccluded()[0]->getImageView()->getImageView();
    bufferInfoOcclusion[0].sampler = material->getOccluded()[0]->getSampler()->getSampler();
    textureInfoColor[5] = bufferInfoOcclusion;

    std::vector<VkDescriptorImageInfo> bufferInfoEmissive(1);
    bufferInfoEmissive[0].imageLayout = material->getEmissive()[0]->getImageView()->getImage()->getImageLayout();
    bufferInfoEmissive[0].imageView = material->getEmissive()[0]->getImageView()->getImageView();
    bufferInfoEmissive[0].sampler = material->getEmissive()[0]->getSampler()->getSampler();
    textureInfoColor[6] = bufferInfoEmissive;

    // TODO: this textures are part of global state for PBR
    std::vector<VkDescriptorImageInfo> bufferInfoIrradiance(1);
    bufferInfoIrradiance[0].imageLayout = material->getDiffuseIBL()->getImageView()->getImage()->getImageLayout();
    bufferInfoIrradiance[0].imageView = material->getDiffuseIBL()->getImageView()->getImageView();
    bufferInfoIrradiance[0].sampler = material->getDiffuseIBL()->getSampler()->getSampler();
    textureInfoColor[7] = bufferInfoIrradiance;

    std::vector<VkDescriptorImageInfo> bufferInfoSpecularIBL(1);
    bufferInfoSpecularIBL[0].imageLayout = material->getSpecularIBL()->getImageView()->getImage()->getImageLayout();
    bufferInfoSpecularIBL[0].imageView = material->getSpecularIBL()->getImageView()->getImageView();
    bufferInfoSpecularIBL[0].sampler = material->getSpecularIBL()->getSampler()->getSampler();
    textureInfoColor[8] = bufferInfoSpecularIBL;

    std::vector<VkDescriptorImageInfo> bufferInfoSpecularBRDF(1);
    bufferInfoSpecularBRDF[0].imageLayout = material->getSpecularBRDF()->getImageView()->getImage()->getImageLayout();
    bufferInfoSpecularBRDF[0].imageView = material->getSpecularBRDF()->getImageView()->getImageView();
    bufferInfoSpecularBRDF[0].sampler = material->getSpecularBRDF()->getSampler()->getSampler();
    textureInfoColor[9] = bufferInfoSpecularBRDF;

    std::vector<VkDescriptorBufferInfo> bufferInfoCoefficients(1);
    // write to binding = 0 for vertex shader
    bufferInfoCoefficients[0].buffer = material->getBufferCoefficients()->getBuffer()[i]->getData();
    bufferInfoCoefficients[0].offset = 0;
    bufferInfoCoefficients[0].range = material->getBufferCoefficients()->getBuffer()[i]->getSize();
    bufferInfoColor[10] = bufferInfoCoefficients;

    std::vector<VkDescriptorBufferInfo> bufferInfoAlphaCutoff(1);
    // write to binding = 0 for vertex shader
    bufferInfoAlphaCutoff[0].buffer = material->getBufferAlphaCutoff()->getBuffer()[i]->getData();
    bufferInfoAlphaCutoff[0].offset = 0;
    bufferInfoAlphaCutoff[0].range = material->getBufferAlphaCutoff()->getBuffer()[i]->getSize();
    bufferInfoColor[11] = bufferInfoAlphaCutoff;

    _descriptorSetPBR->createCustom(i, bufferInfoColor, textureInfoColor);
  }
  material->unregisterUpdate(_descriptorSetPBR);
  material->registerUpdate(_descriptorSetPBR, {{MaterialTexture::COLOR, 1},
                                               {MaterialTexture::NORMAL, 2},
                                               {MaterialTexture::METALLIC, 3},
                                               {MaterialTexture::ROUGHNESS, 4},
                                               {MaterialTexture::OCCLUSION, 5},
                                               {MaterialTexture::EMISSIVE, 6},
                                               {MaterialTexture::IBL_DIFFUSE, 7},
                                               {MaterialTexture::IBL_SPECULAR, 8},
                                               {MaterialTexture::BRDF_SPECULAR, 9}});
}

void Shape3D::enableShadow(bool enable) { _enableShadow = enable; }

void Shape3D::enableLighting(bool enable) { _enableLighting = enable; }

void Shape3D::setMaterial(std::shared_ptr<MaterialColor> material) {
  _materialType = MaterialType::COLOR;
  _updateColorDescriptor(material);
  _material = material;
}

void Shape3D::setMaterial(std::shared_ptr<MaterialPhong> material) {
  _materialType = MaterialType::PHONG;
  _updatePhongDescriptor(material);
  _material = material;
}

void Shape3D::setMaterial(std::shared_ptr<MaterialPBR> material) {
  _materialType = MaterialType::PBR;
  _updatePBRDescriptor(material);
  _material = material;
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
    if (pipeline->getPushConstants().find("constants") != pipeline->getPushConstants().end()) {
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

    _uniformBufferCamera->getBuffer()[currentFrame]->setData(&cameraUBO);

    VkBuffer vertexBuffers[] = {_mesh->getVertexBuffer()->getBuffer()->getData()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame],
                         _mesh->getIndexBuffer()->getBuffer()->getData(), 0, VK_INDEX_TYPE_UINT32);

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
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 1, 1,
                              &_lightManager->getDSGlobalPhong()->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    // PBR
    auto pbrLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                  [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                    return info.first == std::string("pbr");
                                  });
    if (pbrLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 0, 1,
                              &_descriptorSetPBR->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    // global PBR
    auto globalLayoutPBR = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                          return info.first == std::string("globalPBR");
                                        });
    if (globalLayoutPBR != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 1, 1,
                              &_lightManager->getDSGlobalPBR()->getDescriptorSets()[currentFrame], 0, nullptr);
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

    vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame],
                     static_cast<uint32_t>(_mesh->getIndexData().size()), 1, 0, 0, 0);
  };

  auto pipeline = _pipeline[_shapeType][_materialType];
  if (_drawType == DrawType::WIREFRAME) pipeline = _pipelineWireframe[_shapeType][_materialType];
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

  if (pipeline->getPushConstants().find("constants") != pipeline->getPushConstants().end()) {
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

  _cameraUBODepth[lightIndexTotal][face]->getBuffer()[currentFrame]->setData(&cameraMVP);

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