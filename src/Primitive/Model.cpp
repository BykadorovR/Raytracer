#include "Model.h"
#include <unordered_map>
#undef far

void Model3D::enableDepth(bool enable) { _enableDepth = enable; }
bool Model3D::isDepthEnabled() { return _enableDepth; }

Model3D::Model3D(const std::vector<std::shared_ptr<NodeGLTF>>& nodes,
                 const std::vector<std::shared_ptr<Mesh3D>>& meshes,
                 std::shared_ptr<LightManager> lightManager,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<ResourceManager> resourceManager,
                 std::shared_ptr<State> state) {
  _commandBufferTransfer = commandBufferTransfer;
  _state = state;
  _loggerCPU = std::make_shared<LoggerCPU>();
  _nodes = nodes;
  _meshes = meshes;
  _lightManager = lightManager;
  auto settings = _state->getSettings();

  // default material if model doesn't have material at all, we still have to send data to shader
  _defaultMaterialPhong = std::make_shared<MaterialPhong>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
  _defaultMaterialPhong->setBaseColor({resourceManager->getTextureOne()});
  _defaultMaterialPhong->setNormal({resourceManager->getTextureZero()});
  _defaultMaterialPhong->setSpecular({resourceManager->getTextureZero()});
  _defaultMaterialPBR = std::make_shared<MaterialPBR>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
  _defaultMaterialPBR->setBaseColor({resourceManager->getTextureOne()});
  _defaultMaterialPBR->setNormal({resourceManager->getTextureZero()});
  _defaultMaterialPBR->setMetallic({resourceManager->getTextureZero()});
  _defaultMaterialPBR->setRoughness({resourceManager->getTextureZero()});
  _defaultMaterialPBR->setOccluded({resourceManager->getTextureZero()});
  _defaultMaterialPBR->setEmissive({resourceManager->getTextureZero()});
  _defaultMaterialPBR->setDiffuseIBL(resourceManager->getCubemapZero()->getTexture());
  _defaultMaterialPBR->setSpecularIBL(resourceManager->getCubemapZero()->getTexture(),
                                      resourceManager->getTextureZero());
  _defaultMaterialColor = std::make_shared<MaterialColor>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
  _defaultMaterialColor->setBaseColor({resourceManager->getTextureOne()});
  _mesh = std::make_shared<Mesh3D>(state);
  _defaultAnimation = std::make_shared<Animation>(std::vector<std::shared_ptr<NodeGLTF>>{},
                                                  std::vector<std::shared_ptr<SkinGLTF>>{},
                                                  std::vector<std::shared_ptr<AnimationGLTF>>{}, state);
  _animation = _defaultAnimation;
  auto layoutCamera = std::make_shared<DescriptorSetLayout>(state->getDevice());
  layoutCamera->createUniformBuffer();
  auto layoutCameraGeometry = std::make_shared<DescriptorSetLayout>(state->getDevice());
  layoutCameraGeometry->createUniformBuffer(VK_SHADER_STAGE_GEOMETRY_BIT);
  _renderPass = std::make_shared<RenderPass>(_state->getSettings(), _state->getDevice());
  _renderPass->initializeGraphic();
  _renderPassDepth = std::make_shared<RenderPass>(_state->getSettings(), _state->getDevice());
  _renderPassDepth->initializeLightDepth();

  // initialize camera UBO and descriptor sets for draw pass
  // initialize UBO
  _cameraUBOFull = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(BufferMVP),
                                                   _state);

  // setup joints
  {
    _descriptorSetLayoutJoints = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutJoints(1);
    layoutJoints[0].binding = 0;
    layoutJoints[0].descriptorCount = 1;
    layoutJoints[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutJoints[0].pImmutableSamplers = nullptr;
    layoutJoints[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    _descriptorSetLayoutJoints->createCustom(layoutJoints);

    _updateJointsDescriptor();
  }

  // setup Normal
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

    // TODO: we can just have one buffer and put it twice to descriptor

    _descriptorSetNormalsMesh = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                                _descriptorSetLayoutNormalsMesh,
                                                                state->getDescriptorPool(), state->getDevice());
    for (int i = 0; i < state->getSettings()->getMaxFramesInFlight(); i++) {
      std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoNormalsMesh;
      std::vector<VkDescriptorBufferInfo> bufferInfoVertex(1);
      // write to binding = 0 for vertex shader
      bufferInfoVertex[0].buffer = _cameraUBOFull->getBuffer()[i]->getData();
      bufferInfoVertex[0].offset = 0;
      bufferInfoVertex[0].range = sizeof(BufferMVP);
      bufferInfoNormalsMesh[0] = bufferInfoVertex;
      // write for binding = 1 for geometry shader
      std::vector<VkDescriptorBufferInfo> bufferInfoGeometry(1);
      bufferInfoGeometry[0].buffer = _cameraUBOFull->getBuffer()[i]->getData();
      bufferInfoGeometry[0].offset = 0;
      bufferInfoGeometry[0].range = sizeof(BufferMVP);
      bufferInfoNormalsMesh[1] = bufferInfoGeometry;
      _descriptorSetNormalsMesh->createCustom(i, bufferInfoNormalsMesh, {});
    }

    // initialize Normal (per vertex)
    auto shader = std::make_shared<Shader>(_state);
    shader->add("shaders/shape/cubeNormal_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/shape/cubeNormal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    shader->add("shaders/shape/cubeNormal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

    _pipelineNormalMesh = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineNormalMesh->createGraphic3D(
        VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
        std::vector{std::pair{std::string("normal"), _descriptorSetLayoutNormalsMesh}}, {},
        _mesh->getBindingDescription(),
        _mesh->getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                         {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, normal)},
                                         {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)}}),
        _renderPass);

    // it's kind of pointless, but if material is set, double sided property can be handled
    _pipelineNormalMeshCullOff = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineNormalMeshCullOff->createGraphic3D(
        VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
        std::vector{std::pair{std::string("normal"), _descriptorSetLayoutNormalsMesh}}, {},
        _mesh->getBindingDescription(),
        _mesh->getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                         {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, normal)},
                                         {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)}}),
        _renderPass);

    // initialize Tangent (per vertex)
    {
      auto shader = std::make_shared<Shader>(_state);
      shader->add("shaders/shape/cubeTangent_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/shape/cubeNormal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shader->add("shaders/shape/cubeNormal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

      _pipelineTangentMesh = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      _pipelineTangentMesh->createGraphic3D(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
          std::vector{std::pair{std::string("normal"), _descriptorSetLayoutNormalsMesh}}, {},
          _mesh->getBindingDescription(),
          _mesh->getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                           {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, normal)},
                                           {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)}}),
          _renderPass);

      // it's kind of pointless, but if material is set, double sided property can be handled
      _pipelineTangentMeshCullOff = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      _pipelineTangentMeshCullOff->createGraphic3D(
          VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
          std::vector{std::pair{std::string("normal"), _descriptorSetLayoutNormalsMesh}}, {},
          _mesh->getBindingDescription(),
          _mesh->getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                           {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, normal)},
                                           {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)}}),
          _renderPass);
    }
  }

  // setup color
  {
    _descriptorSetLayoutColor = std::make_shared<DescriptorSetLayout>(_state->getDevice());
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
    _descriptorSetLayoutColor->createCustom(layoutColor);

    _updateColorDescriptor({_defaultMaterialColor});

    // initialize Color
    {
      auto shader = std::make_shared<Shader>(_state);
      shader->add("shaders/model/modelColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/model/modelColor_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

      _pipeline[MaterialType::COLOR] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
      std::vector<std::tuple<VkFormat, uint32_t>> attributes = {
          {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
          {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)},
          {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)},
          {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex3D, jointIndices)},
          {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex3D, jointWeights)}};

      _pipeline[MaterialType::COLOR]->createGraphic3D(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          {{"color", _descriptorSetLayoutColor}, {"joints", _descriptorSetLayoutJoints}}, {},
          _mesh->getBindingDescription(), _mesh->getAttributeDescriptions(attributes), _renderPass);

      _pipelineCullOff[MaterialType::COLOR] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
      _pipelineCullOff[MaterialType::COLOR]->createGraphic3D(
          VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          {{"color", _descriptorSetLayoutColor}, {"joints", _descriptorSetLayoutJoints}}, {},
          _mesh->getBindingDescription(), _mesh->getAttributeDescriptions(attributes), _renderPass);

      _pipelineWireframe[MaterialType::COLOR] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
      _pipelineWireframe[MaterialType::COLOR]->createGraphic3D(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          {{"color", _descriptorSetLayoutColor}, {"joints", _descriptorSetLayoutJoints}}, {},
          _mesh->getBindingDescription(), _mesh->getAttributeDescriptions(attributes), _renderPass);
    }
  }

  // setup Phong
  {
    _descriptorSetLayoutPhong = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutPhong(6);
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
    layoutPhong[5].binding = 5;
    layoutPhong[5].descriptorCount = 1;
    layoutPhong[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPhong[5].pImmutableSamplers = nullptr;
    layoutPhong[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    _descriptorSetLayoutPhong->createCustom(layoutPhong);

    _updatePhongDescriptor({_defaultMaterialPhong});

    auto shader = std::make_shared<Shader>(_state);
    shader->add("shaders/model/modelPhong_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/model/modelPhong_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[MaterialType::PHONG] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["constants"] = LightPush::getPushConstant();

    _pipeline[MaterialType::PHONG]->createGraphic3D(VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
                                                    {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                     shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                    {{"phong", _descriptorSetLayoutPhong},
                                                     {"joints", _descriptorSetLayoutJoints},
                                                     {"globalPhong", lightManager->getDSLGlobalPhong()}},
                                                    defaultPushConstants, _mesh->getBindingDescription(),
                                                    _mesh->getAttributeDescriptions(), _renderPass);

    _pipelineCullOff[MaterialType::PHONG] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    _pipelineCullOff[MaterialType::PHONG]->createGraphic3D(VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
                                                           {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                            shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                           {{"phong", _descriptorSetLayoutPhong},
                                                            {"joints", _descriptorSetLayoutJoints},
                                                            {"globalPhong", lightManager->getDSLGlobalPhong()}},
                                                           defaultPushConstants, _mesh->getBindingDescription(),
                                                           _mesh->getAttributeDescriptions(), _renderPass);

    _pipelineWireframe[MaterialType::PHONG] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    _pipelineWireframe[MaterialType::PHONG]->createGraphic3D(VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
                                                             {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                              shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                             {{"phong", _descriptorSetLayoutPhong},
                                                              {"joints", _descriptorSetLayoutJoints},
                                                              {"globalPhong", lightManager->getDSLGlobalPhong()}},
                                                             defaultPushConstants, _mesh->getBindingDescription(),
                                                             _mesh->getAttributeDescriptions(), _renderPass);
  }

  // setup PBR
  {
    _descriptorSetLayoutPBR = std::make_shared<DescriptorSetLayout>(_state->getDevice());
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

    _descriptorSetLayoutPBR->createCustom(layoutPBR);

    _updatePBRDescriptor({_defaultMaterialPBR});

    // initialize PBR
    {
      auto shader = std::make_shared<Shader>(_state);
      shader->add("shaders/model/modelPBR_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/model/modelPBR_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

      _pipeline[MaterialType::PBR] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
      std::map<std::string, VkPushConstantRange> defaultPushConstants;
      defaultPushConstants["constants"] = LightPush::getPushConstant();

      _pipeline[MaterialType::PBR]->createGraphic3D(VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
                                                    {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                     shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                    {{"pbr", _descriptorSetLayoutPBR},
                                                     {"joints", _descriptorSetLayoutJoints},
                                                     {"globalPBR", lightManager->getDSLGlobalPBR()}},
                                                    defaultPushConstants, _mesh->getBindingDescription(),
                                                    _mesh->getAttributeDescriptions(), _renderPass);

      _pipelineCullOff[MaterialType::PBR] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
      _pipelineCullOff[MaterialType::PBR]->createGraphic3D(VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
                                                           {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                            shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                           {{"pbr", _descriptorSetLayoutPBR},
                                                            {"joints", _descriptorSetLayoutJoints},
                                                            {"globalPBR", lightManager->getDSLGlobalPBR()}},
                                                           defaultPushConstants, _mesh->getBindingDescription(),
                                                           _mesh->getAttributeDescriptions(), _renderPass);

      _pipelineWireframe[MaterialType::PBR] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
      _pipelineWireframe[MaterialType::PBR]->createGraphic3D(VK_CULL_MODE_NONE, VK_POLYGON_MODE_LINE,
                                                             {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                              shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                             {{"pbr", _descriptorSetLayoutPBR},
                                                              {"joints", _descriptorSetLayoutJoints},
                                                              {"globalPBR", lightManager->getDSLGlobalPBR()}},
                                                             defaultPushConstants, _mesh->getBindingDescription(),
                                                             _mesh->getAttributeDescriptions(), _renderPass);
    }
  }

  // initialize camera UBO and descriptor sets for shadow
  // initialize UBO
  int lightNumber = _state->getSettings()->getMaxDirectionalLights() + _state->getSettings()->getMaxPointLights();
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
  // initialize descriptor sets
  {
    for (int i = 0; i < _state->getSettings()->getMaxDirectionalLights(); i++) {
      auto cameraSet = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(), layoutCamera,
                                                       _state->getDescriptorPool(), _state->getDevice());
      cameraSet->createUniformBuffer(_cameraUBODepth[i][0]);

      _descriptorSetCameraDepth.push_back({cameraSet});
    }

    for (int i = 0; i < _state->getSettings()->getMaxPointLights(); i++) {
      std::vector<std::shared_ptr<DescriptorSet>> facesSet(6);
      for (int j = 0; j < 6; j++) {
        facesSet[j] = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(), layoutCamera,
                                                      _state->getDescriptorPool(), _state->getDevice());
        facesSet[j]->createUniformBuffer(_cameraUBODepth[i + _state->getSettings()->getMaxDirectionalLights()][j]);
      }
      _descriptorSetCameraDepth.push_back(facesSet);
    }
  }

  // initialize depth directional
  {
    auto shader = std::make_shared<Shader>(_state);
    shader->add("shaders/model/modelDepth_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    _pipelineDirectional = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineDirectional->createGraphic3DShadow(
        VK_CULL_MODE_NONE, {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT)},
        {{"depth", layoutCamera}, {"joints", _descriptorSetLayoutJoints}}, {}, _mesh->getBindingDescription(),
        _mesh->getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                         {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex3D, jointIndices)},
                                         {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex3D, jointWeights)}}),
        _renderPassDepth);
  }

  // initialize depth point
  {
    auto shader = std::make_shared<Shader>(_state);
    shader->add("shaders/model/modelDepth_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/model/modelDepth_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelinePoint = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["constants"] = DepthConstants::getPushConstant(0);
    _pipelinePoint->createGraphic3DShadow(
        VK_CULL_MODE_NONE,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        {{"depth", layoutCamera}, {"joints", _descriptorSetLayoutJoints}}, defaultPushConstants,
        _mesh->getBindingDescription(),
        _mesh->getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                         {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex3D, jointIndices)},
                                         {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex3D, jointWeights)}}),
        _renderPassDepth);
  }
}

void Model3D::_updateJointsDescriptor() {
  _descriptorSetJoints.resize(_animation->getJointMatricesBuffer().size());
  for (int skin = 0; skin < _animation->getJointMatricesBuffer().size(); skin++) {
    _descriptorSetJoints[skin] = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                                 _descriptorSetLayoutJoints,
                                                                 _state->getDescriptorPool(), _state->getDevice());
    for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
      std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
      // write for binding = 0 for joint matrices
      std::vector<VkDescriptorBufferInfo> bufferInfoJointMatrices(1);
      bufferInfoJointMatrices[0].buffer = _animation->getJointMatricesBuffer()[skin][i]->getData();
      bufferInfoJointMatrices[0].offset = 0;
      bufferInfoJointMatrices[0].range = _animation->getJointMatricesBuffer()[skin][i]->getSize();
      bufferInfoColor[0] = bufferInfoJointMatrices;
      _descriptorSetJoints[skin]->createCustom(i, bufferInfoColor, {});
    }
  }
}

void Model3D::_updatePBRDescriptor(std::vector<std::shared_ptr<MaterialPBR>> materials) {
  for (int material = 0; material < materials.size(); material++) {
    if (_descriptorSetPBR.size() <= material)
      _descriptorSetPBR.push_back(std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                                  _descriptorSetLayoutPBR, _state->getDescriptorPool(),
                                                                  _state->getDevice()));
    for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
      std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
      std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
      std::vector<VkDescriptorBufferInfo> bufferInfoCamera(1);
      // write to binding = 0 for vertex shader
      bufferInfoCamera[0].buffer = _cameraUBOFull->getBuffer()[i]->getData();
      bufferInfoCamera[0].offset = 0;
      bufferInfoCamera[0].range = sizeof(BufferMVP);
      bufferInfoColor[0] = bufferInfoCamera;

      // write for binding = 1 for textures
      std::vector<VkDescriptorImageInfo> bufferInfoBaseColor(1);
      bufferInfoBaseColor[0].imageLayout =
          materials[material]->getBaseColor()[0]->getImageView()->getImage()->getImageLayout();
      bufferInfoBaseColor[0].imageView = materials[material]->getBaseColor()[0]->getImageView()->getImageView();
      bufferInfoBaseColor[0].sampler = materials[material]->getBaseColor()[0]->getSampler()->getSampler();
      textureInfoColor[1] = bufferInfoBaseColor;

      std::vector<VkDescriptorImageInfo> bufferInfoNormal(1);
      bufferInfoNormal[0].imageLayout =
          materials[material]->getNormal()[0]->getImageView()->getImage()->getImageLayout();
      bufferInfoNormal[0].imageView = materials[material]->getNormal()[0]->getImageView()->getImageView();
      bufferInfoNormal[0].sampler = materials[material]->getNormal()[0]->getSampler()->getSampler();
      textureInfoColor[2] = bufferInfoNormal;

      std::vector<VkDescriptorImageInfo> bufferInfoMetallic(1);
      bufferInfoMetallic[0].imageLayout =
          materials[material]->getMetallic()[0]->getImageView()->getImage()->getImageLayout();
      bufferInfoMetallic[0].imageView = materials[material]->getMetallic()[0]->getImageView()->getImageView();
      bufferInfoMetallic[0].sampler = materials[material]->getMetallic()[0]->getSampler()->getSampler();
      textureInfoColor[3] = bufferInfoMetallic;

      std::vector<VkDescriptorImageInfo> bufferInfoRoughness(1);
      bufferInfoRoughness[0].imageLayout =
          materials[material]->getRoughness()[0]->getImageView()->getImage()->getImageLayout();
      bufferInfoRoughness[0].imageView = materials[material]->getRoughness()[0]->getImageView()->getImageView();
      bufferInfoRoughness[0].sampler = materials[material]->getRoughness()[0]->getSampler()->getSampler();
      textureInfoColor[4] = bufferInfoRoughness;

      std::vector<VkDescriptorImageInfo> bufferInfoOcclusion(1);
      bufferInfoOcclusion[0].imageLayout =
          materials[material]->getOccluded()[0]->getImageView()->getImage()->getImageLayout();
      bufferInfoOcclusion[0].imageView = materials[material]->getOccluded()[0]->getImageView()->getImageView();
      bufferInfoOcclusion[0].sampler = materials[material]->getOccluded()[0]->getSampler()->getSampler();
      textureInfoColor[5] = bufferInfoOcclusion;

      std::vector<VkDescriptorImageInfo> bufferInfoEmissive(1);
      bufferInfoEmissive[0].imageLayout =
          materials[material]->getEmissive()[0]->getImageView()->getImage()->getImageLayout();
      bufferInfoEmissive[0].imageView = materials[material]->getEmissive()[0]->getImageView()->getImageView();
      bufferInfoEmissive[0].sampler = materials[material]->getEmissive()[0]->getSampler()->getSampler();
      textureInfoColor[6] = bufferInfoEmissive;

      // TODO: this textures are part of global state for PBR
      std::vector<VkDescriptorImageInfo> bufferInfoIrradiance(1);
      bufferInfoIrradiance[0].imageLayout =
          materials[material]->getDiffuseIBL()->getImageView()->getImage()->getImageLayout();
      bufferInfoIrradiance[0].imageView = materials[material]->getDiffuseIBL()->getImageView()->getImageView();
      bufferInfoIrradiance[0].sampler = materials[material]->getDiffuseIBL()->getSampler()->getSampler();
      textureInfoColor[7] = bufferInfoIrradiance;

      std::vector<VkDescriptorImageInfo> bufferInfoSpecularIBL(1);
      bufferInfoSpecularIBL[0].imageLayout =
          materials[material]->getSpecularIBL()->getImageView()->getImage()->getImageLayout();
      bufferInfoSpecularIBL[0].imageView = materials[material]->getSpecularIBL()->getImageView()->getImageView();
      bufferInfoSpecularIBL[0].sampler = materials[material]->getSpecularIBL()->getSampler()->getSampler();
      textureInfoColor[8] = bufferInfoSpecularIBL;

      std::vector<VkDescriptorImageInfo> bufferInfoSpecularBRDF(1);
      bufferInfoSpecularBRDF[0].imageLayout =
          materials[material]->getSpecularBRDF()->getImageView()->getImage()->getImageLayout();
      bufferInfoSpecularBRDF[0].imageView = materials[material]->getSpecularBRDF()->getImageView()->getImageView();
      bufferInfoSpecularBRDF[0].sampler = materials[material]->getSpecularBRDF()->getSampler()->getSampler();
      textureInfoColor[9] = bufferInfoSpecularBRDF;

      std::vector<VkDescriptorBufferInfo> bufferInfoAlphaCutoff(1);
      // write to binding = 0 for vertex shader
      bufferInfoAlphaCutoff[0].buffer = materials[material]->getBufferAlphaCutoff()->getBuffer()[i]->getData();
      bufferInfoAlphaCutoff[0].offset = 0;
      bufferInfoAlphaCutoff[0].range = materials[material]->getBufferAlphaCutoff()->getBuffer()[i]->getSize();
      bufferInfoColor[10] = bufferInfoAlphaCutoff;

      std::vector<VkDescriptorBufferInfo> bufferInfoCoefficients(1);
      // write to binding = 0 for vertex shader
      bufferInfoCoefficients[0].buffer = materials[material]->getBufferCoefficients()->getBuffer()[i]->getData();
      bufferInfoCoefficients[0].offset = 0;
      bufferInfoCoefficients[0].range = materials[material]->getBufferCoefficients()->getBuffer()[i]->getSize();
      bufferInfoColor[11] = bufferInfoCoefficients;

      _descriptorSetPBR[material]->createCustom(i, bufferInfoColor, textureInfoColor);
    }
  }
}

void Model3D::_updatePhongDescriptor(std::vector<std::shared_ptr<MaterialPhong>> materials) {
  for (int material = 0; material < materials.size(); material++) {
    if (_descriptorSetPhong.size() <= material)
      _descriptorSetPhong.push_back(std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                                    _descriptorSetLayoutPhong,
                                                                    _state->getDescriptorPool(), _state->getDevice()));
    for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
      std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
      std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
      std::vector<VkDescriptorBufferInfo> bufferInfoCamera(1);
      // write to binding = 0 for vertex shader
      bufferInfoCamera[0].buffer = _cameraUBOFull->getBuffer()[i]->getData();
      bufferInfoCamera[0].offset = 0;
      bufferInfoCamera[0].range = sizeof(BufferMVP);
      bufferInfoColor[0] = bufferInfoCamera;

      // write for binding = 1 for base color
      std::vector<VkDescriptorImageInfo> bufferInfoBase(1);
      bufferInfoBase[0].imageLayout =
          materials[material]->getBaseColor()[0]->getImageView()->getImage()->getImageLayout();
      bufferInfoBase[0].imageView = materials[material]->getBaseColor()[0]->getImageView()->getImageView();
      bufferInfoBase[0].sampler = materials[material]->getBaseColor()[0]->getSampler()->getSampler();
      textureInfoColor[1] = bufferInfoBase;

      // write for binding = 2 for normal color
      std::vector<VkDescriptorImageInfo> bufferInfoNormal(1);
      bufferInfoNormal[0].imageLayout =
          materials[material]->getNormal()[0]->getImageView()->getImage()->getImageLayout();
      bufferInfoNormal[0].imageView = materials[material]->getNormal()[0]->getImageView()->getImageView();
      bufferInfoNormal[0].sampler = materials[material]->getNormal()[0]->getSampler()->getSampler();
      textureInfoColor[2] = bufferInfoNormal;

      std::vector<VkDescriptorImageInfo> bufferInfoSpecular(1);
      bufferInfoSpecular[0].imageLayout =
          materials[material]->getSpecular()[0]->getImageView()->getImage()->getImageLayout();
      bufferInfoSpecular[0].imageView = materials[material]->getSpecular()[0]->getImageView()->getImageView();
      bufferInfoSpecular[0].sampler = materials[material]->getSpecular()[0]->getSampler()->getSampler();
      textureInfoColor[3] = bufferInfoSpecular;

      std::vector<VkDescriptorBufferInfo> bufferInfoAlphaCutoff(1);
      // write to binding = 0 for vertex shader
      bufferInfoAlphaCutoff[0].buffer = materials[material]->getBufferAlphaCutoff()->getBuffer()[i]->getData();
      bufferInfoAlphaCutoff[0].offset = 0;
      bufferInfoAlphaCutoff[0].range = materials[material]->getBufferAlphaCutoff()->getBuffer()[i]->getSize();
      bufferInfoColor[4] = bufferInfoAlphaCutoff;

      std::vector<VkDescriptorBufferInfo> bufferInfoCoefficients(1);
      // write to binding = 0 for vertex shader
      bufferInfoCoefficients[0].buffer = materials[material]->getBufferCoefficients()->getBuffer()[i]->getData();
      bufferInfoCoefficients[0].offset = 0;
      bufferInfoCoefficients[0].range = materials[material]->getBufferCoefficients()->getBuffer()[i]->getSize();
      bufferInfoColor[5] = bufferInfoCoefficients;

      _descriptorSetPhong[material]->createCustom(i, bufferInfoColor, textureInfoColor);
    }
  }
}

void Model3D::_updateColorDescriptor(std::vector<std::shared_ptr<MaterialColor>> materials) {
  for (int material = 0; material < materials.size(); material++) {
    if (_descriptorSetColor.size() <= material)
      _descriptorSetColor.push_back(std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                                    _descriptorSetLayoutColor,
                                                                    _state->getDescriptorPool(), _state->getDevice()));
    for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
      std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
      std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
      std::vector<VkDescriptorBufferInfo> bufferInfoCamera(1);
      // write to binding = 0 for vertex shader
      bufferInfoCamera[0].buffer = _cameraUBOFull->getBuffer()[i]->getData();
      bufferInfoCamera[0].offset = 0;
      bufferInfoCamera[0].range = sizeof(BufferMVP);
      bufferInfoColor[0] = bufferInfoCamera;

      // write for binding = 1 for textures
      std::vector<VkDescriptorImageInfo> bufferInfoTexture(1);
      bufferInfoTexture[0].imageLayout =
          materials[material]->getBaseColor()[0]->getImageView()->getImage()->getImageLayout();
      bufferInfoTexture[0].imageView = materials[material]->getBaseColor()[0]->getImageView()->getImageView();
      bufferInfoTexture[0].sampler = materials[material]->getBaseColor()[0]->getSampler()->getSampler();
      textureInfoColor[1] = bufferInfoTexture;
      _descriptorSetColor[material]->createCustom(i, bufferInfoColor, textureInfoColor);
    }
  }
}

void Model3D::setMaterial(std::vector<std::shared_ptr<MaterialColor>> materials) {
  _materials.clear();
  for (auto& material : materials) {
    _materials.push_back(material);
  }
  _materialType = MaterialType::COLOR;
  _updateColorDescriptor(materials);
}

void Model3D::setMaterial(std::vector<std::shared_ptr<MaterialPhong>> materials) {
  _materials.clear();
  for (auto& material : materials) {
    _materials.push_back(material);
  }
  _materialType = MaterialType::PHONG;
  _updatePhongDescriptor(materials);
}

void Model3D::setMaterial(std::vector<std::shared_ptr<MaterialPBR>> materials) {
  _materials.clear();
  for (auto& material : materials) {
    _materials.push_back(material);
  }
  _materialType = MaterialType::PBR;
  _updatePBRDescriptor(materials);
}

void Model3D::setDrawType(DrawType drawType) { _drawType = drawType; }

MaterialType Model3D::getMaterialType() { return _materialType; }

DrawType Model3D::getDrawType() { return _drawType; }

void Model3D::setAnimation(std::shared_ptr<Animation> animation) {
  _animation = animation;
  _updateJointsDescriptor();
}

void Model3D::_drawNode(std::shared_ptr<CommandBuffer> commandBuffer,
                        std::shared_ptr<Pipeline> pipeline,
                        std::shared_ptr<Pipeline> pipelineCullOff,
                        std::shared_ptr<DescriptorSet> cameraDS,
                        std::shared_ptr<UniformBuffer> cameraUBO,
                        glm::mat4 view,
                        glm::mat4 projection,
                        std::shared_ptr<NodeGLTF> node) {
  int currentFrame = _state->getFrameInFlight();
  if (node->mesh >= 0 && _meshes[node->mesh]->getPrimitives().size() > 0) {
    VkBuffer vertexBuffers[] = {_meshes[node->mesh]->getVertexBuffer()->getBuffer()->getData()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame],
                         _meshes[node->mesh]->getIndexBuffer()->getBuffer()->getData(), 0, VK_INDEX_TYPE_UINT32);

    glm::mat4 nodeMatrix = node->getLocalMatrix();
    std::shared_ptr<NodeGLTF> currentParent = node->parent;
    while (currentParent) {
      nodeMatrix = currentParent->getLocalMatrix() * nodeMatrix;
      currentParent = currentParent->parent;
    }
    // pass this matrix to uniforms
    BufferMVP cameraMVP{};
    cameraMVP.model = _model * nodeMatrix;
    cameraMVP.view = view;
    cameraMVP.projection = projection;

    void* data;
    vkMapMemory(_state->getDevice()->getLogicalDevice(), cameraUBO->getBuffer()[currentFrame]->getMemory(), 0,
                sizeof(cameraMVP), 0, &data);
    memcpy(data, &cameraMVP, sizeof(cameraMVP));
    vkUnmapMemory(_state->getDevice()->getLogicalDevice(), cameraUBO->getBuffer()[currentFrame]->getMemory());

    auto pipelineLayout = pipeline->getDescriptorSetLayout();

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

    // depth
    auto depthLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                    [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                      return info.first == std::string("depth");
                                    });
    if (depthLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 0, 1, &cameraDS->getDescriptorSets()[currentFrame], 0,
                              nullptr);
    }

    // joints
    auto jointLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                    [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                      return info.first == std::string("joints");
                                    });

    if (jointLayout != pipelineLayout.end()) {
      // if node->skin == -1 then use 0 index that contains identity matrix because of animation default behavior
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 1, 1,
                              &_descriptorSetJoints[std::max(0, node->skin)]->getDescriptorSets()[currentFrame], 0,
                              nullptr);
    }

    for (MeshPrimitive primitive : _meshes[node->mesh]->getPrimitives()) {
      if (primitive.indexCount > 0) {
        std::shared_ptr<Material> material = _defaultMaterialPhong;
        int materialIndex = 0;
        // Get the texture index for this primitive
        if (_materials.size() > 0) {
          material = _materials.front();
          if (primitive.materialIndex >= 0 && primitive.materialIndex < _materials.size()) {
            material = _materials[primitive.materialIndex];
            materialIndex = primitive.materialIndex;
          }
        }

        // color
        auto layoutColor = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                          return info.first == std::string("color");
                                        });
        if (layoutColor != pipelineLayout.end()) {
          vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipeline->getPipelineLayout(), 0, 1,
                                  &_descriptorSetColor[materialIndex]->getDescriptorSets()[currentFrame], 0, nullptr);
        }

        // phong
        auto layoutPhong = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                          return info.first == std::string("phong");
                                        });
        if (layoutPhong != pipelineLayout.end()) {
          vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipeline->getPipelineLayout(), 0, 1,
                                  &_descriptorSetPhong[materialIndex]->getDescriptorSets()[currentFrame], 0, nullptr);
        }

        // pbr
        auto layoutPBR = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                        return info.first == std::string("pbr");
                                      });
        if (layoutPBR != pipelineLayout.end()) {
          vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipeline->getPipelineLayout(), 0, 1,
                                  &_descriptorSetPBR[materialIndex]->getDescriptorSets()[currentFrame], 0, nullptr);
        }

        auto currentPipeline = pipeline;
        // by default double side is false
        if (material->getDoubleSided()) currentPipeline = pipelineCullOff;
        vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                          currentPipeline->getPipeline());
        vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], primitive.indexCount, 1, primitive.firstIndex,
                         0, 0);
      }
    }
  }

  for (auto& child : node->children) {
    _drawNode(commandBuffer, pipeline, pipelineCullOff, cameraDS, cameraUBO, view, projection, child);
  }
}

void Model3D::enableShadow(bool enable) { _enableShadow = enable; }

void Model3D::enableLighting(bool enable) { _enableLighting = enable; }

void Model3D::draw(std::tuple<int, int> resolution,
                   std::shared_ptr<Camera> camera,
                   std::shared_ptr<CommandBuffer> commandBuffer) {
  auto pipeline = _pipeline[_materialType];
  auto pipelineCullOff = _pipelineCullOff[_materialType];
  if (_drawType == DrawType::WIREFRAME) pipeline = _pipelineWireframe[_materialType];
  if (_drawType == DrawType::NORMAL) {
    pipeline = _pipelineNormalMesh;
    pipelineCullOff = _pipelineNormalMeshCullOff;
  }
  if (_drawType == DrawType::TANGENT) {
    pipeline = _pipelineTangentMesh;
    pipelineCullOff = _pipelineTangentMeshCullOff;
  }

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

  if (pipeline->getPushConstants().find("constants") != pipeline->getPushConstants().end()) {
    LightPush pushConstants;
    pushConstants.enableShadow = _enableShadow;
    pushConstants.enableLighting = _enableLighting;
    pushConstants.cameraPosition = camera->getEye();
    vkCmdPushConstants(commandBuffer->getCommandBuffer()[_state->getFrameInFlight()], pipeline->getPipelineLayout(),
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LightPush), &pushConstants);
  }

  auto pipelineLayout = pipeline->getDescriptorSetLayout();

  // global Phong
  auto globalLayoutPhong = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                          return info.first == std::string("globalPhong");
                                        });
  if (globalLayoutPhong != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->getPipelineLayout(), 2, 1,
                            &_lightManager->getDSGlobalPhong()->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  // global PBR
  auto globalLayoutPBR = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                        return info.first == std::string("globalPBR");
                                      });
  if (globalLayoutPBR != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->getPipelineLayout(), 2, 1,
                            &_lightManager->getDSGlobalPBR()->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  // Render all nodes at top-level
  for (auto& node : _nodes) {
    _drawNode(commandBuffer, pipeline, pipelineCullOff, nullptr, _cameraUBOFull, camera->getView(),
              camera->getProjection(), node);
  }
}

void Model3D::drawShadow(LightType lightType, int lightIndex, int face, std::shared_ptr<CommandBuffer> commandBuffer) {
  if (_enableDepth == false) return;

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
    if (pipeline->getPushConstants().find("constants") != pipeline->getPushConstants().end()) {
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
  // Render all nodes at top-level
  for (auto& node : _nodes) {
    _drawNode(commandBuffer, pipeline, pipeline, _descriptorSetCameraDepth[lightIndexTotal][face],
              _cameraUBODepth[lightIndexTotal][face], view, projection, node);
  }
}