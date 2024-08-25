#include "Primitive/Terrain.h"
#undef far
#undef near

struct LoDConstants {
  int minTessellationLevel;
  int maxTessellationLevel;
  float near;
  float far;
  static VkPushConstantRange getPushConstant() {
    VkPushConstantRange pushConstant{};
    // this push constant range starts at the beginning
    // this push constant range takes up the size of a MeshPushConstants struct
    pushConstant.size = sizeof(LoDConstants);
    // this push constant range is accessible only in the vertex shader
    pushConstant.stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    return pushConstant;
  }
};

struct PatchConstants {
  int patchDimX;
  int patchDimY;
  float heightScale;
  float heightShift;
  static VkPushConstantRange getPushConstant() {
    VkPushConstantRange pushConstant{};
    // this push constant range starts at the beginning
    // this push constant range takes up the size of a MeshPushConstants struct
    pushConstant.offset = sizeof(LoDConstants);
    pushConstant.size = sizeof(PatchConstants);
    // this push constant range is accessible only in the vertex shader
    pushConstant.stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    return pushConstant;
  }
};

struct HeightLevels {
  alignas(16) float heightLevels[4];
  alignas(16) int patchEdge;
  alignas(16) int showLOD;
  alignas(16) int enableShadow;
  alignas(16) int enableLighting;
  alignas(16) glm::vec3 cameraPosition;
  static VkPushConstantRange getPushConstant() {
    VkPushConstantRange pushConstant{};
    // this push constant range starts at the beginning
    // this push constant range takes up the size of a MeshPushConstants struct
    pushConstant.offset = sizeof(LoDConstants) + sizeof(PatchConstants);
    pushConstant.size = sizeof(HeightLevels);
    // this push constant range is accessible only in the vertex shader
    pushConstant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    return pushConstant;
  }
};

Terrain::Terrain(std::shared_ptr<BufferImage> heightMap,
                 std::pair<int, int> patchNumber,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<LightManager> lightManager,
                 std::shared_ptr<State> state) {
  setName("Terrain");
  _state = state;
  _patchNumber = patchNumber;
  _lightManager = lightManager;

  // needed for layout
  _defaultMaterialColor = std::make_shared<MaterialColor>(MaterialTarget::TERRAIN, commandBufferTransfer, state);
  _defaultMaterialPhong = std::make_shared<MaterialPhong>(MaterialTarget::TERRAIN, commandBufferTransfer, state);
  _defaultMaterialPBR = std::make_shared<MaterialPBR>(MaterialTarget::TERRAIN, commandBufferTransfer, state);
  _heightMap = std::make_shared<Texture>(heightMap, _state->getSettings()->getLoadTextureAuxilaryFormat(),
                                         VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, VK_FILTER_LINEAR, commandBufferTransfer,
                                         state);
  auto [width, height] = _heightMap->getImageView()->getImage()->getResolution();
  std::vector<Vertex3D> vertices;
  _mesh = std::make_shared<Mesh3D>(state);
  for (unsigned y = 0; y < patchNumber.second; y++) {
    for (unsigned x = 0; x < patchNumber.first; x++) {
      // define patch: 4 points (square)
      Vertex3D vertex1{};
      vertex1.pos = glm::vec3(-width / 2.0f + width * x / (float)patchNumber.first, 0.f,
                              -height / 2.0f + height * y / (float)patchNumber.second);
      vertex1.texCoord = glm::vec2(x, y);
      vertices.push_back(vertex1);

      Vertex3D vertex2{};
      vertex2.pos = glm::vec3(-width / 2.0f + width * (x + 1) / (float)patchNumber.first, 0.f,
                              -height / 2.0f + height * y / (float)patchNumber.second);
      vertex2.texCoord = glm::vec2(x + 1, y);
      vertices.push_back(vertex2);

      Vertex3D vertex3{};
      vertex3.pos = glm::vec3(-width / 2.0f + width * x / (float)patchNumber.first, 0.f,
                              -height / 2.0f + height * (y + 1) / (float)patchNumber.second);
      vertex3.texCoord = glm::vec2(x, y + 1);
      vertices.push_back(vertex3);

      Vertex3D vertex4{};
      vertex4.pos = glm::vec3(-width / 2.0f + width * (x + 1) / (float)patchNumber.first, 0.f,
                              -height / 2.0f + height * (y + 1) / (float)patchNumber.second);
      vertex4.texCoord = glm::vec2(x + 1, y + 1);
      vertices.push_back(vertex4);
    }
  }
  _mesh->setVertices(vertices, commandBufferTransfer);

  std::map<std::string, VkPushConstantRange> defaultPushConstants;
  defaultPushConstants["control"] = LoDConstants::getPushConstant();
  defaultPushConstants["evaluate"] = PatchConstants::getPushConstant();
  defaultPushConstants["fragment"] = HeightLevels::getPushConstant();

  _renderPass = std::make_shared<RenderPass>(_state->getSettings(), _state->getDevice());
  _renderPass->initializeGraphic();
  _renderPassShadow = std::make_shared<RenderPass>(_state->getSettings(), _state->getDevice());
  _renderPassShadow->initializeLightDepth();

  _cameraBuffer = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(BufferMVP),
                                                  state);
  for (int i = 0; i < state->getSettings()->getMaxDirectionalLights(); i++) {
    _cameraBufferDepth.push_back(
        {std::make_shared<UniformBuffer>(state->getSettings()->getMaxFramesInFlight(), sizeof(BufferMVP), state)});
  }

  for (int i = 0; i < state->getSettings()->getMaxPointLights(); i++) {
    std::vector<std::shared_ptr<UniformBuffer>> facesBuffer(6);
    for (int j = 0; j < 6; j++) {
      facesBuffer[j] = std::make_shared<UniformBuffer>(state->getSettings()->getMaxFramesInFlight(), sizeof(BufferMVP),
                                                       state);
    }
    _cameraBufferDepth.push_back(facesBuffer);
  }

  // layout for Shadows
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutShadows(3);
    layoutShadows[0].binding = 0;
    layoutShadows[0].descriptorCount = 1;
    layoutShadows[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutShadows[0].pImmutableSamplers = nullptr;
    layoutShadows[0].stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    layoutShadows[1].binding = 1;
    layoutShadows[1].descriptorCount = 1;
    layoutShadows[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutShadows[1].pImmutableSamplers = nullptr;
    layoutShadows[1].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    layoutShadows[2].binding = 2;
    layoutShadows[2].descriptorCount = 1;
    layoutShadows[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutShadows[2].pImmutableSamplers = nullptr;
    layoutShadows[2].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    descriptorSetLayout->createCustom(layoutShadows);
    _descriptorSetLayoutShadows.push_back({"shadows", descriptorSetLayout});

    for (int d = 0; d < _state->getSettings()->getMaxDirectionalLights(); d++) {
      auto descriptorSetShadows = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                                  descriptorSetLayout, state->getDescriptorPool(),
                                                                  state->getDevice());
      for (int i = 0; i < state->getSettings()->getMaxFramesInFlight(); i++) {
        std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoNormalsMesh;
        std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
        std::vector<VkDescriptorBufferInfo> bufferInfoTesControl(1);
        bufferInfoTesControl[0].buffer = _cameraBufferDepth[d][0]->getBuffer()[i]->getData();
        bufferInfoTesControl[0].offset = 0;
        bufferInfoTesControl[0].range = sizeof(BufferMVP);
        bufferInfoNormalsMesh[0] = bufferInfoTesControl;

        std::vector<VkDescriptorBufferInfo> bufferInfoTesEval(1);
        bufferInfoTesEval[0].buffer = _cameraBufferDepth[d][0]->getBuffer()[i]->getData();
        bufferInfoTesEval[0].offset = 0;
        bufferInfoTesEval[0].range = sizeof(BufferMVP);
        bufferInfoNormalsMesh[1] = bufferInfoTesEval;

        std::vector<VkDescriptorImageInfo> bufferInfoHeightMap(1);
        bufferInfoHeightMap[0].imageLayout = _heightMap->getImageView()->getImage()->getImageLayout();
        bufferInfoHeightMap[0].imageView = _heightMap->getImageView()->getImageView();
        bufferInfoHeightMap[0].sampler = _heightMap->getSampler()->getSampler();
        textureInfoColor[2] = bufferInfoHeightMap;
        descriptorSetShadows->createCustom(i, bufferInfoNormalsMesh, textureInfoColor);
      }
      _descriptorSetCameraDepth.push_back({descriptorSetShadows});
    }

    for (int p = 0; p < _state->getSettings()->getMaxPointLights(); p++) {
      std::vector<std::shared_ptr<DescriptorSet>> facesSet;
      for (int f = 0; f < 6; f++) {
        auto descriptorSetShadows = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                                    descriptorSetLayout, state->getDescriptorPool(),
                                                                    state->getDevice());
        for (int i = 0; i < state->getSettings()->getMaxFramesInFlight(); i++) {
          std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoNormalsMesh;
          std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
          std::vector<VkDescriptorBufferInfo> bufferInfoTesControl(1);
          bufferInfoTesControl[0].buffer =
              _cameraBufferDepth[p + _state->getSettings()->getMaxDirectionalLights()][f]->getBuffer()[i]->getData();
          bufferInfoTesControl[0].offset = 0;
          bufferInfoTesControl[0].range = sizeof(BufferMVP);
          bufferInfoNormalsMesh[0] = bufferInfoTesControl;

          std::vector<VkDescriptorBufferInfo> bufferInfoTesEval(1);
          bufferInfoTesEval[0].buffer =
              _cameraBufferDepth[p + _state->getSettings()->getMaxDirectionalLights()][f]->getBuffer()[i]->getData();
          bufferInfoTesEval[0].offset = 0;
          bufferInfoTesEval[0].range = sizeof(BufferMVP);
          bufferInfoNormalsMesh[1] = bufferInfoTesEval;

          std::vector<VkDescriptorImageInfo> bufferInfoHeightMap(1);
          bufferInfoHeightMap[0].imageLayout = _heightMap->getImageView()->getImage()->getImageLayout();
          bufferInfoHeightMap[0].imageView = _heightMap->getImageView()->getImageView();
          bufferInfoHeightMap[0].sampler = _heightMap->getSampler()->getSampler();
          textureInfoColor[2] = bufferInfoHeightMap;
          descriptorSetShadows->createCustom(i, bufferInfoNormalsMesh, textureInfoColor);
        }
        facesSet.push_back(descriptorSetShadows);
      }
      _descriptorSetCameraDepth.push_back(facesSet);
    }

    // initialize Shadows (Directional)
    {
      auto shader = std::make_shared<Shader>(state);
      shader->add("shaders/terrain/terrainDepth_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/terrain/terrainDepth_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shader->add("shaders/terrain/terrainDepth_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

      std::map<std::string, VkPushConstantRange> defaultPushConstants;
      defaultPushConstants["control"] = LoDConstants::getPushConstant();
      defaultPushConstants["evaluate"] = PatchConstants::getPushConstant();

      _pipelineDirectional = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      _pipelineDirectional->createGraphicTerrainShadowGPU(
          VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
          _descriptorSetLayoutShadows, defaultPushConstants, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPassShadow);
    }

    // initialize Shadows (Point)
    {
      auto shader = std::make_shared<Shader>(state);
      shader->add("shaders/terrain/terrainDepth_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/terrain/terrainDepth_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shader->add("shaders/terrain/terrainDepth_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      shader->add("shaders/terrain/terrainDepth_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

      std::map<std::string, VkPushConstantRange> defaultPushConstants;
      defaultPushConstants["control"] = LoDConstants::getPushConstant();
      defaultPushConstants["evaluate"] = PatchConstants::getPushConstant();
      defaultPushConstants["fragment"] = DepthConstants::getPushConstant(sizeof(LoDConstants) + sizeof(PatchConstants));

      _pipelinePoint = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      _pipelinePoint->createGraphicTerrainShadowGPU(
          VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          _descriptorSetLayoutShadows, defaultPushConstants, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPassShadow);
    }
  }

  // layout for Normal
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutNormal(4);
    layoutNormal[0].binding = 0;
    layoutNormal[0].descriptorCount = 1;
    layoutNormal[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutNormal[0].pImmutableSamplers = nullptr;
    layoutNormal[0].stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    layoutNormal[1].binding = 1;
    layoutNormal[1].descriptorCount = 1;
    layoutNormal[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutNormal[1].pImmutableSamplers = nullptr;
    layoutNormal[1].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    layoutNormal[2].binding = 2;
    layoutNormal[2].descriptorCount = 1;
    layoutNormal[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutNormal[2].pImmutableSamplers = nullptr;
    layoutNormal[2].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    layoutNormal[3].binding = 3;
    layoutNormal[3].descriptorCount = 1;
    layoutNormal[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutNormal[3].pImmutableSamplers = nullptr;
    layoutNormal[3].stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT;
    descriptorSetLayout->createCustom(layoutNormal);
    _descriptorSetLayoutNormalsMesh.push_back({"normal", descriptorSetLayout});
    _descriptorSetNormal = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                           descriptorSetLayout, state->getDescriptorPool(),
                                                           state->getDevice());
    for (int i = 0; i < state->getSettings()->getMaxFramesInFlight(); i++) {
      std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoNormalsMesh;
      std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
      std::vector<VkDescriptorBufferInfo> bufferInfoTesControl(1);
      bufferInfoTesControl[0].buffer = _cameraBuffer->getBuffer()[i]->getData();
      bufferInfoTesControl[0].offset = 0;
      bufferInfoTesControl[0].range = sizeof(BufferMVP);
      bufferInfoNormalsMesh[0] = bufferInfoTesControl;

      std::vector<VkDescriptorBufferInfo> bufferInfoTesEval(1);
      bufferInfoTesEval[0].buffer = _cameraBuffer->getBuffer()[i]->getData();
      bufferInfoTesEval[0].offset = 0;
      bufferInfoTesEval[0].range = sizeof(BufferMVP);
      bufferInfoNormalsMesh[1] = bufferInfoTesEval;

      std::vector<VkDescriptorImageInfo> bufferInfoHeightMap(1);
      bufferInfoHeightMap[0].imageLayout = _heightMap->getImageView()->getImage()->getImageLayout();
      bufferInfoHeightMap[0].imageView = _heightMap->getImageView()->getImageView();
      bufferInfoHeightMap[0].sampler = _heightMap->getSampler()->getSampler();
      textureInfoColor[2] = bufferInfoHeightMap;

      // write for binding = 1 for geometry shader
      std::vector<VkDescriptorBufferInfo> bufferInfoGeometry(1);
      bufferInfoGeometry[0].buffer = _cameraBuffer->getBuffer()[i]->getData();
      bufferInfoGeometry[0].offset = 0;
      bufferInfoGeometry[0].range = sizeof(BufferMVP);
      bufferInfoNormalsMesh[3] = bufferInfoGeometry;
      _descriptorSetNormal->createCustom(i, bufferInfoNormalsMesh, textureInfoColor);
    }

    // initialize Normal (per vertex)
    {
      auto shaderNormal = std::make_shared<Shader>(state);
      shaderNormal->add("shaders/terrain/terrainColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shaderNormal->add("shaders/terrain/terrainNormal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shaderNormal->add("shaders/terrain/terrainColor_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shaderNormal->add("shaders/terrain/terrainNormal_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      shaderNormal->add("shaders/terrain/terrainNormal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

      _pipelineNormalMesh = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      _pipelineNormalMesh->createGraphicTerrain(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
          {shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
          _descriptorSetLayoutNormalsMesh, defaultPushConstants, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPass);
    }

    // initialize Tangent (per vertex)
    {
      auto shaderNormal = std::make_shared<Shader>(state);
      shaderNormal->add("shaders/terrain/terrainColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shaderNormal->add("shaders/terrain/terrainNormal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shaderNormal->add("shaders/terrain/terrainColor_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shaderNormal->add("shaders/terrain/terrainTangent_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      shaderNormal->add("shaders/terrain/terrainNormal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

      _pipelineTangentMesh = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      _pipelineTangentMesh->createGraphicTerrain(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
          {shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
          _descriptorSetLayoutNormalsMesh, defaultPushConstants, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPass);
    }
  }

  // layout for Color
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutColor(4);
    layoutColor[0].binding = 0;
    layoutColor[0].descriptorCount = 1;
    layoutColor[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutColor[0].pImmutableSamplers = nullptr;
    layoutColor[0].stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    layoutColor[1].binding = 1;
    layoutColor[1].descriptorCount = 1;
    layoutColor[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutColor[1].pImmutableSamplers = nullptr;
    layoutColor[1].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    layoutColor[2].binding = 2;
    layoutColor[2].descriptorCount = 1;
    layoutColor[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutColor[2].pImmutableSamplers = nullptr;
    layoutColor[2].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    layoutColor[3].binding = 3;
    layoutColor[3].descriptorCount = 4;
    layoutColor[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutColor[3].pImmutableSamplers = nullptr;
    layoutColor[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayout->createCustom(layoutColor);
    _descriptorSetLayout[MaterialType::COLOR].push_back({"color", descriptorSetLayout});
    _descriptorSetColor = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                          descriptorSetLayout, state->getDescriptorPool(),
                                                          state->getDevice());
    // update descriptor set in setMaterial

    // initialize Color
    {
      auto shader = std::make_shared<Shader>(state);
      shader->add("shaders/terrain/terrainColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/terrain/terrainColor_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shader->add("shaders/terrain/terrainColor_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shader->add("shaders/terrain/terrainColor_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      _pipeline[MaterialType::COLOR] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      _pipeline[MaterialType::COLOR]->createGraphicTerrain(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
          _descriptorSetLayout[MaterialType::COLOR], defaultPushConstants, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPass);

      _pipelineWireframe[MaterialType::COLOR] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      _pipelineWireframe[MaterialType::COLOR]->createGraphicTerrain(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
          _descriptorSetLayout[MaterialType::COLOR], defaultPushConstants, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPass);
    }
  }
  // layout for Phong
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutPhong(7);
    layoutPhong[0].binding = 0;
    layoutPhong[0].descriptorCount = 1;
    layoutPhong[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPhong[0].pImmutableSamplers = nullptr;
    layoutPhong[0].stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    layoutPhong[1].binding = 1;
    layoutPhong[1].descriptorCount = 1;
    layoutPhong[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPhong[1].pImmutableSamplers = nullptr;
    layoutPhong[1].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    layoutPhong[2].binding = 2;
    layoutPhong[2].descriptorCount = 1;
    layoutPhong[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPhong[2].pImmutableSamplers = nullptr;
    layoutPhong[2].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    layoutPhong[3].binding = 3;
    layoutPhong[3].descriptorCount = 4;
    layoutPhong[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPhong[3].pImmutableSamplers = nullptr;
    layoutPhong[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPhong[4].binding = 4;
    layoutPhong[4].descriptorCount = 4;
    layoutPhong[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPhong[4].pImmutableSamplers = nullptr;
    layoutPhong[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPhong[5].binding = 5;
    layoutPhong[5].descriptorCount = 4;
    layoutPhong[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPhong[5].pImmutableSamplers = nullptr;
    layoutPhong[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPhong[6].binding = 6;
    layoutPhong[6].descriptorCount = 1;
    layoutPhong[6].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPhong[6].pImmutableSamplers = nullptr;
    layoutPhong[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayout->createCustom(layoutPhong);

    _descriptorSetLayout[MaterialType::PHONG].push_back({"phong", descriptorSetLayout});
    _descriptorSetLayout[MaterialType::PHONG].push_back({"globalPhong", lightManager->getDSLGlobalTerrainPhong()});
    _descriptorSetPhong = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                          descriptorSetLayout, state->getDescriptorPool(),
                                                          state->getDevice());
    // update descriptor set in setMaterial

    // initialize Phong
    {
      auto shader = std::make_shared<Shader>(state);
      shader->add("shaders/terrain/terrainColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/terrain/terrainPhong_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shader->add("shaders/terrain/terrainColor_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shader->add("shaders/terrain/terrainPhong_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      _pipeline[MaterialType::PHONG] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      _pipeline[MaterialType::PHONG]->createGraphicTerrain(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
          _descriptorSetLayout[MaterialType::PHONG], defaultPushConstants, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPass);

      _pipelineWireframe[MaterialType::PHONG] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      _pipelineWireframe[MaterialType::PHONG]->createGraphicTerrain(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
          _descriptorSetLayout[MaterialType::PHONG], defaultPushConstants, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPass);
    }
  }
  // layout for PBR
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutPBR(14);
    layoutPBR[0].binding = 0;
    layoutPBR[0].descriptorCount = 1;
    layoutPBR[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPBR[0].pImmutableSamplers = nullptr;
    layoutPBR[0].stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    layoutPBR[1].binding = 1;
    layoutPBR[1].descriptorCount = 1;
    layoutPBR[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPBR[1].pImmutableSamplers = nullptr;
    layoutPBR[1].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    layoutPBR[2].binding = 2;
    layoutPBR[2].descriptorCount = 1;
    layoutPBR[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[2].pImmutableSamplers = nullptr;
    layoutPBR[2].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    layoutPBR[3].binding = 3;
    layoutPBR[3].descriptorCount = 4;
    layoutPBR[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[3].pImmutableSamplers = nullptr;
    layoutPBR[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[4].binding = 4;
    layoutPBR[4].descriptorCount = 4;
    layoutPBR[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[4].pImmutableSamplers = nullptr;
    layoutPBR[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[5].binding = 5;
    layoutPBR[5].descriptorCount = 4;
    layoutPBR[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[5].pImmutableSamplers = nullptr;
    layoutPBR[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[6].binding = 6;
    layoutPBR[6].descriptorCount = 4;
    layoutPBR[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[6].pImmutableSamplers = nullptr;
    layoutPBR[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[7].binding = 7;
    layoutPBR[7].descriptorCount = 4;
    layoutPBR[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[7].pImmutableSamplers = nullptr;
    layoutPBR[7].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[8].binding = 8;
    layoutPBR[8].descriptorCount = 4;
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
    layoutPBR[10].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[10].pImmutableSamplers = nullptr;
    layoutPBR[10].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[11].binding = 11;
    layoutPBR[11].descriptorCount = 1;
    layoutPBR[11].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[11].pImmutableSamplers = nullptr;
    layoutPBR[11].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[12].binding = 12;
    layoutPBR[12].descriptorCount = 1;
    layoutPBR[12].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPBR[12].pImmutableSamplers = nullptr;
    layoutPBR[12].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[13].binding = 13;
    layoutPBR[13].descriptorCount = 1;
    layoutPBR[13].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPBR[13].pImmutableSamplers = nullptr;
    layoutPBR[13].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayout->createCustom(layoutPBR);
    _descriptorSetLayout[MaterialType::PBR].push_back({"pbr", descriptorSetLayout});
    _descriptorSetLayout[MaterialType::PBR].push_back({"globalPBR", lightManager->getDSLGlobalTerrainPBR()});

    _descriptorSetPBR = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                        descriptorSetLayout, state->getDescriptorPool(),
                                                        state->getDevice());
    // update descriptor set in setMaterial

    // initialize PBR
    {
      auto shader = std::make_shared<Shader>(state);
      shader->add("shaders/terrain/terrainColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/terrain/terrainPBR_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shader->add("shaders/terrain/terrainColor_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shader->add("shaders/terrain/terrainPhong_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      _pipeline[MaterialType::PBR] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      _pipeline[MaterialType::PBR]->createGraphicTerrain(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
          _descriptorSetLayout[MaterialType::PBR], defaultPushConstants, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPass);

      _pipelineWireframe[MaterialType::PBR] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      _pipelineWireframe[MaterialType::PBR]->createGraphicTerrain(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
          _descriptorSetLayout[MaterialType::PBR], defaultPushConstants, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPass);
    }
  }
}

void Terrain::_updateColorDescriptor(std::shared_ptr<MaterialColor> material) {
  std::vector<VkDescriptorImageInfo> colorImageInfo(_state->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
    std::vector<VkDescriptorBufferInfo> bufferInfoCameraControl(1);
    bufferInfoCameraControl[0].buffer = _cameraBuffer->getBuffer()[i]->getData();
    bufferInfoCameraControl[0].offset = 0;
    bufferInfoCameraControl[0].range = sizeof(BufferMVP);
    bufferInfoColor[0] = bufferInfoCameraControl;

    std::vector<VkDescriptorBufferInfo> bufferInfoCameraEval(1);
    bufferInfoCameraEval[0].buffer = _cameraBuffer->getBuffer()[i]->getData();
    bufferInfoCameraEval[0].offset = 0;
    bufferInfoCameraEval[0].range = sizeof(BufferMVP);
    bufferInfoColor[1] = bufferInfoCameraEval;

    std::vector<VkDescriptorImageInfo> textureHeightmap(1);
    textureHeightmap[0].imageLayout = _heightMap->getImageView()->getImage()->getImageLayout();
    textureHeightmap[0].imageView = _heightMap->getImageView()->getImageView();
    textureHeightmap[0].sampler = _heightMap->getSampler()->getSampler();
    textureInfoColor[2] = textureHeightmap;

    std::vector<VkDescriptorImageInfo> textureBaseColor(material->getBaseColor().size());
    for (int j = 0; j < material->getBaseColor().size(); j++) {
      textureBaseColor[j].imageLayout = material->getBaseColor()[j]->getImageView()->getImage()->getImageLayout();
      textureBaseColor[j].imageView = material->getBaseColor()[j]->getImageView()->getImageView();
      textureBaseColor[j].sampler = material->getBaseColor()[j]->getSampler()->getSampler();
    }
    textureInfoColor[3] = textureBaseColor;

    _descriptorSetColor->createCustom(i, bufferInfoColor, textureInfoColor);
  }
  _material->unregisterUpdate(_descriptorSetColor);
  material->registerUpdate(_descriptorSetColor, {{MaterialTexture::COLOR, 3}});
}

void Terrain::_updatePhongDescriptor(std::shared_ptr<MaterialPhong> material) {
  std::vector<VkDescriptorImageInfo> colorImageInfo(_state->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
    std::vector<VkDescriptorBufferInfo> bufferInfoCameraControl(1);
    bufferInfoCameraControl[0].buffer = _cameraBuffer->getBuffer()[i]->getData();
    bufferInfoCameraControl[0].offset = 0;
    bufferInfoCameraControl[0].range = sizeof(BufferMVP);
    bufferInfoColor[0] = bufferInfoCameraControl;

    std::vector<VkDescriptorBufferInfo> bufferInfoCameraEval(1);
    bufferInfoCameraEval[0].buffer = _cameraBuffer->getBuffer()[i]->getData();
    bufferInfoCameraEval[0].offset = 0;
    bufferInfoCameraEval[0].range = sizeof(BufferMVP);
    bufferInfoColor[1] = bufferInfoCameraEval;

    std::vector<VkDescriptorImageInfo> textureHeightmap(1);
    textureHeightmap[0].imageLayout = _heightMap->getImageView()->getImage()->getImageLayout();
    textureHeightmap[0].imageView = _heightMap->getImageView()->getImageView();
    textureHeightmap[0].sampler = _heightMap->getSampler()->getSampler();
    textureInfoColor[2] = textureHeightmap;

    std::vector<VkDescriptorImageInfo> textureBaseColor(material->getBaseColor().size());
    for (int j = 0; j < material->getBaseColor().size(); j++) {
      textureBaseColor[j].imageLayout = material->getBaseColor()[j]->getImageView()->getImage()->getImageLayout();
      textureBaseColor[j].imageView = material->getBaseColor()[j]->getImageView()->getImageView();
      textureBaseColor[j].sampler = material->getBaseColor()[j]->getSampler()->getSampler();
    }
    textureInfoColor[3] = textureBaseColor;

    std::vector<VkDescriptorImageInfo> textureBaseNormal(material->getNormal().size());
    for (int j = 0; j < material->getNormal().size(); j++) {
      textureBaseNormal[j].imageLayout = material->getNormal()[j]->getImageView()->getImage()->getImageLayout();
      textureBaseNormal[j].imageView = material->getNormal()[j]->getImageView()->getImageView();
      textureBaseNormal[j].sampler = material->getNormal()[j]->getSampler()->getSampler();
    }
    textureInfoColor[4] = textureBaseNormal;

    std::vector<VkDescriptorImageInfo> textureBaseSpecular(material->getSpecular().size());
    for (int j = 0; j < material->getSpecular().size(); j++) {
      textureBaseSpecular[j].imageLayout = material->getSpecular()[j]->getImageView()->getImage()->getImageLayout();
      textureBaseSpecular[j].imageView = material->getSpecular()[j]->getImageView()->getImageView();
      textureBaseSpecular[j].sampler = material->getSpecular()[j]->getSampler()->getSampler();
    }
    textureInfoColor[5] = textureBaseSpecular;

    std::vector<VkDescriptorBufferInfo> bufferInfoCoefficients(1);
    // write to binding = 0 for vertex shader
    bufferInfoCoefficients[0].buffer = material->getBufferCoefficients()->getBuffer()[i]->getData();
    bufferInfoCoefficients[0].offset = 0;
    bufferInfoCoefficients[0].range = material->getBufferCoefficients()->getBuffer()[i]->getSize();
    bufferInfoColor[6] = bufferInfoCoefficients;
    _descriptorSetPhong->createCustom(i, bufferInfoColor, textureInfoColor);
  }
  _material->unregisterUpdate(_descriptorSetPhong);
  material->registerUpdate(_descriptorSetPhong,
                           {{MaterialTexture::COLOR, 3}, {MaterialTexture::NORMAL, 4}, {MaterialTexture::SPECULAR, 5}});
}

void Terrain::_updatePBRDescriptor(std::shared_ptr<MaterialPBR> material) {
  std::vector<VkDescriptorImageInfo> colorImageInfo(_state->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
    std::vector<VkDescriptorBufferInfo> bufferInfoCameraControl(1);
    bufferInfoCameraControl[0].buffer = _cameraBuffer->getBuffer()[i]->getData();
    bufferInfoCameraControl[0].offset = 0;
    bufferInfoCameraControl[0].range = sizeof(BufferMVP);
    bufferInfoColor[0] = bufferInfoCameraControl;

    std::vector<VkDescriptorBufferInfo> bufferInfoCameraEval(1);
    bufferInfoCameraEval[0].buffer = _cameraBuffer->getBuffer()[i]->getData();
    bufferInfoCameraEval[0].offset = 0;
    bufferInfoCameraEval[0].range = sizeof(BufferMVP);
    bufferInfoColor[1] = bufferInfoCameraEval;

    std::vector<VkDescriptorImageInfo> textureHeightmap(1);
    textureHeightmap[0].imageLayout = _heightMap->getImageView()->getImage()->getImageLayout();
    textureHeightmap[0].imageView = _heightMap->getImageView()->getImageView();
    textureHeightmap[0].sampler = _heightMap->getSampler()->getSampler();
    textureInfoColor[2] = textureHeightmap;

    std::vector<VkDescriptorImageInfo> textureBaseColor(material->getBaseColor().size());
    for (int j = 0; j < material->getBaseColor().size(); j++) {
      textureBaseColor[j].imageLayout = material->getBaseColor()[j]->getImageView()->getImage()->getImageLayout();
      textureBaseColor[j].imageView = material->getBaseColor()[j]->getImageView()->getImageView();
      textureBaseColor[j].sampler = material->getBaseColor()[j]->getSampler()->getSampler();
    }
    textureInfoColor[3] = textureBaseColor;

    std::vector<VkDescriptorImageInfo> textureBaseNormal(material->getNormal().size());
    for (int j = 0; j < material->getNormal().size(); j++) {
      textureBaseNormal[j].imageLayout = material->getNormal()[j]->getImageView()->getImage()->getImageLayout();
      textureBaseNormal[j].imageView = material->getNormal()[j]->getImageView()->getImageView();
      textureBaseNormal[j].sampler = material->getNormal()[j]->getSampler()->getSampler();
    }
    textureInfoColor[4] = textureBaseNormal;

    std::vector<VkDescriptorImageInfo> textureBaseMetallic(material->getMetallic().size());
    for (int j = 0; j < material->getMetallic().size(); j++) {
      textureBaseMetallic[j].imageLayout = material->getMetallic()[j]->getImageView()->getImage()->getImageLayout();
      textureBaseMetallic[j].imageView = material->getMetallic()[j]->getImageView()->getImageView();
      textureBaseMetallic[j].sampler = material->getMetallic()[j]->getSampler()->getSampler();
    }
    textureInfoColor[5] = textureBaseMetallic;

    std::vector<VkDescriptorImageInfo> textureBaseRoughness(material->getRoughness().size());
    for (int j = 0; j < material->getRoughness().size(); j++) {
      textureBaseRoughness[j].imageLayout = material->getRoughness()[j]->getImageView()->getImage()->getImageLayout();
      textureBaseRoughness[j].imageView = material->getRoughness()[j]->getImageView()->getImageView();
      textureBaseRoughness[j].sampler = material->getRoughness()[j]->getSampler()->getSampler();
    }
    textureInfoColor[6] = textureBaseRoughness;

    std::vector<VkDescriptorImageInfo> textureBaseOcclusion(material->getOccluded().size());
    for (int j = 0; j < material->getOccluded().size(); j++) {
      textureBaseOcclusion[j].imageLayout = material->getOccluded()[j]->getImageView()->getImage()->getImageLayout();
      textureBaseOcclusion[j].imageView = material->getOccluded()[j]->getImageView()->getImageView();
      textureBaseOcclusion[j].sampler = material->getOccluded()[j]->getSampler()->getSampler();
    }
    textureInfoColor[7] = textureBaseOcclusion;

    std::vector<VkDescriptorImageInfo> textureBaseEmissive(material->getEmissive().size());
    for (int j = 0; j < material->getEmissive().size(); j++) {
      textureBaseEmissive[j].imageLayout = material->getEmissive()[j]->getImageView()->getImage()->getImageLayout();
      textureBaseEmissive[j].imageView = material->getEmissive()[j]->getImageView()->getImageView();
      textureBaseEmissive[j].sampler = material->getEmissive()[j]->getSampler()->getSampler();
    }
    textureInfoColor[8] = textureBaseEmissive;

    // TODO: this textures are part of global state for PBR
    std::vector<VkDescriptorImageInfo> bufferInfoIrradiance(1);
    bufferInfoIrradiance[0].imageLayout = material->getDiffuseIBL()->getImageView()->getImage()->getImageLayout();
    bufferInfoIrradiance[0].imageView = material->getDiffuseIBL()->getImageView()->getImageView();
    bufferInfoIrradiance[0].sampler = material->getDiffuseIBL()->getSampler()->getSampler();
    textureInfoColor[9] = bufferInfoIrradiance;

    std::vector<VkDescriptorImageInfo> bufferInfoSpecularIBL(1);
    bufferInfoSpecularIBL[0].imageLayout = material->getSpecularIBL()->getImageView()->getImage()->getImageLayout();
    bufferInfoSpecularIBL[0].imageView = material->getSpecularIBL()->getImageView()->getImageView();
    bufferInfoSpecularIBL[0].sampler = material->getSpecularIBL()->getSampler()->getSampler();
    textureInfoColor[10] = bufferInfoSpecularIBL;

    std::vector<VkDescriptorImageInfo> bufferInfoSpecularBRDF(1);
    bufferInfoSpecularBRDF[0].imageLayout = material->getSpecularBRDF()->getImageView()->getImage()->getImageLayout();
    bufferInfoSpecularBRDF[0].imageView = material->getSpecularBRDF()->getImageView()->getImageView();
    bufferInfoSpecularBRDF[0].sampler = material->getSpecularBRDF()->getSampler()->getSampler();
    textureInfoColor[11] = bufferInfoSpecularBRDF;

    std::vector<VkDescriptorBufferInfo> bufferInfoCoefficients(1);
    // write to binding = 0 for vertex shader
    bufferInfoCoefficients[0].buffer = material->getBufferCoefficients()->getBuffer()[i]->getData();
    bufferInfoCoefficients[0].offset = 0;
    bufferInfoCoefficients[0].range = material->getBufferCoefficients()->getBuffer()[i]->getSize();
    bufferInfoColor[12] = bufferInfoCoefficients;

    std::vector<VkDescriptorBufferInfo> bufferInfoAlphaCutoff(1);
    // write to binding = 0 for vertex shader
    bufferInfoAlphaCutoff[0].buffer = material->getBufferAlphaCutoff()->getBuffer()[i]->getData();
    bufferInfoAlphaCutoff[0].offset = 0;
    bufferInfoAlphaCutoff[0].range = material->getBufferAlphaCutoff()->getBuffer()[i]->getSize();
    bufferInfoColor[13] = bufferInfoAlphaCutoff;

    _descriptorSetPBR->createCustom(i, bufferInfoColor, textureInfoColor);
  }
  _material->unregisterUpdate(_descriptorSetPBR);
  material->registerUpdate(_descriptorSetPBR, {{MaterialTexture::COLOR, 3},
                                               {MaterialTexture::NORMAL, 4},
                                               {MaterialTexture::METALLIC, 5},
                                               {MaterialTexture::ROUGHNESS, 6},
                                               {MaterialTexture::OCCLUSION, 7},
                                               {MaterialTexture::EMISSIVE, 8},
                                               {MaterialTexture::IBL_DIFFUSE, 9},
                                               {MaterialTexture::IBL_SPECULAR, 10},
                                               {MaterialTexture::BRDF_SPECULAR, 11}});
}

void Terrain::setTessellationLevel(int min, int max) {
  _minTessellationLevel = min;
  _maxTessellationLevel = max;
}

void Terrain::setDisplayDistance(int min, int max) {
  _minDistance = min;
  _maxDistance = max;
}

void Terrain::setHeight(float scale, float shift) {
  _heightScale = scale;
  _heightShift = shift;
}

void Terrain::setColorHeightLevels(std::array<float, 4> levels) { _heightLevels = levels; }

void Terrain::enableShadow(bool enable) { _enableShadow = enable; }

void Terrain::enableLighting(bool enable) { _enableLighting = enable; }

void Terrain::setMaterial(std::shared_ptr<MaterialColor> material) {
  _material = material;
  _materialType = MaterialType::COLOR;
  _updateColorDescriptor(material);
}

void Terrain::setMaterial(std::shared_ptr<MaterialPhong> material) {
  _material = material;
  _materialType = MaterialType::PHONG;
  _updatePhongDescriptor(material);
}

void Terrain::setMaterial(std::shared_ptr<MaterialPBR> material) {
  _material = material;
  _materialType = MaterialType::PBR;
  _updatePBRDescriptor(material);
}

void Terrain::setDrawType(DrawType drawType) { _drawType = drawType; }

DrawType Terrain::getDrawType() { return _drawType; }

void Terrain::showLoD(bool enable) { _showLoD = enable; }

void Terrain::patchEdge(bool enable) { _enableEdge = enable; }

void Terrain::draw(std::tuple<int, int> resolution,
                   std::shared_ptr<Camera> camera,
                   std::shared_ptr<CommandBuffer> commandBuffer) {
  int currentFrame = _state->getFrameInFlight();
  auto drawTerrain = [&](std::shared_ptr<Pipeline> pipeline) {
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

    if (pipeline->getPushConstants().find("control") != pipeline->getPushConstants().end()) {
      LoDConstants pushConstants;
      pushConstants.near = _minDistance;
      pushConstants.far = _maxDistance;
      pushConstants.minTessellationLevel = _minTessellationLevel;
      pushConstants.maxTessellationLevel = _maxTessellationLevel;
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0, sizeof(LoDConstants), &pushConstants);
    }

    if (pipeline->getPushConstants().find("evaluate") != pipeline->getPushConstants().end()) {
      PatchConstants pushConstants;
      pushConstants.patchDimX = _patchNumber.first;
      pushConstants.patchDimY = _patchNumber.second;
      pushConstants.heightScale = _heightScale;
      pushConstants.heightShift = _heightShift;
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, sizeof(LoDConstants), sizeof(PatchConstants),
                         &pushConstants);
    }

    if (pipeline->getPushConstants().find("fragment") != pipeline->getPushConstants().end()) {
      HeightLevels pushConstants;
      std::copy(std::begin(_heightLevels), std::end(_heightLevels), std::begin(pushConstants.heightLevels));
      pushConstants.patchEdge = _enableEdge;
      pushConstants.showLOD = _showLoD;
      pushConstants.enableShadow = _enableShadow;
      pushConstants.enableLighting = _enableLighting;
      pushConstants.cameraPosition = camera->getEye();
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(LoDConstants) + sizeof(PatchConstants),
                         sizeof(HeightLevels), &pushConstants);
    }

    // same buffer to both tessellation shaders because we're not going to change camera between these 2 stages
    BufferMVP cameraUBO{};
    cameraUBO.model = _model;
    cameraUBO.view = camera->getView();
    cameraUBO.projection = camera->getProjection();

    _cameraBuffer->getBuffer()[currentFrame]->setData(&cameraUBO);

    VkBuffer vertexBuffers[] = {_mesh->getVertexBuffer()->getBuffer()->getData()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

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
                              &_lightManager->getDSGlobalTerrainPhong()->getDescriptorSets()[currentFrame], 0, nullptr);
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
                              &_lightManager->getDSGlobalTerrainPBR()->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    // normals and tangents
    auto normalTangentLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                            [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                              return info.first == std::string("normal");
                                            });
    if (normalTangentLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 0, 1,
                              &_descriptorSetNormal->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    vkCmdDraw(commandBuffer->getCommandBuffer()[currentFrame], _mesh->getVertexData().size(), 1, 0, 0);
  };

  auto pipeline = _pipeline[_materialType];
  if (_drawType == DrawType::WIREFRAME) pipeline = _pipelineWireframe[_materialType];
  if (_drawType == DrawType::NORMAL) pipeline = _pipelineNormalMesh;
  if (_drawType == DrawType::TANGENT) pipeline = _pipelineTangentMesh;
  drawTerrain(pipeline);
}

void Terrain::drawShadow(LightType lightType, int lightIndex, int face, std::shared_ptr<CommandBuffer> commandBuffer) {
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

  if (pipeline->getPushConstants().find("control") != pipeline->getPushConstants().end()) {
    LoDConstants pushConstants;
    pushConstants.near = _minDistance;
    pushConstants.far = _maxDistance;
    pushConstants.minTessellationLevel = _minTessellationLevel;
    pushConstants.maxTessellationLevel = _maxTessellationLevel;
    vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                       VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0, sizeof(LoDConstants), &pushConstants);
  }

  if (pipeline->getPushConstants().find("evaluate") != pipeline->getPushConstants().end()) {
    PatchConstants pushConstants;
    pushConstants.patchDimX = _patchNumber.first;
    pushConstants.patchDimY = _patchNumber.second;
    pushConstants.heightScale = _heightScale;
    pushConstants.heightShift = _heightShift;
    vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                       VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, sizeof(LoDConstants), sizeof(PatchConstants),
                       &pushConstants);
  }

  if (pipeline->getPushConstants().find("fragment") != pipeline->getPushConstants().end()) {
    DepthConstants pushConstants;
    pushConstants.lightPosition = _lightManager->getPointLights()[lightIndex]->getPosition();
    // light camera
    pushConstants.far = 100.f;
    vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                       VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(LoDConstants) + sizeof(PatchConstants),
                       sizeof(DepthConstants), &pushConstants);
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

  // same buffer to both tessellation shaders because we're not going to change camera between these 2 stages
  BufferMVP cameraUBO{};
  cameraUBO.model = _model;
  cameraUBO.view = view;
  cameraUBO.projection = projection;

  _cameraBufferDepth[lightIndexTotal][face]->getBuffer()[currentFrame]->setData(&cameraUBO);

  VkBuffer vertexBuffers[] = {_mesh->getVertexBuffer()->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  auto pipelineLayout = pipeline->getDescriptorSetLayout();
  auto normalLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                   [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                     return info.first == std::string("shadows");
                                   });
  if (normalLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(),
        0, 1, &_descriptorSetCameraDepth[lightIndexTotal][face]->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  vkCmdDraw(commandBuffer->getCommandBuffer()[currentFrame], _mesh->getVertexData().size(), 1, 0, 0);
}