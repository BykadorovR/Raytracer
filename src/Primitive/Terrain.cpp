module Terrain;
import Shader;

namespace VulkanEngine {
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
                 std::vector<VkFormat> renderFormat,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<LightManager> lightManager,
                 std::shared_ptr<State> state) {
  _state = state;
  _patchNumber = patchNumber;
  _lightManager = lightManager;

  // needed for layout
  _defaultMaterialColor = std::make_shared<MaterialColor>(MaterialTarget::TERRAIN, commandBufferTransfer, state);
  _defaultMaterialPhong = std::make_shared<MaterialPhong>(MaterialTarget::TERRAIN, commandBufferTransfer, state);
  _defaultMaterialPBR = std::make_shared<MaterialPBR>(MaterialTarget::TERRAIN, commandBufferTransfer, state);
  _heightMap = std::make_shared<Texture>(heightMap, _state->getSettings()->getLoadTextureAuxilaryFormat(),
                                         VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer, state);
  auto [width, height] = _heightMap->getImageView()->getImage()->getResolution();
  // vertex generation
  // TODO: replace Vertex3D and Mesh3D with Terrain mesh without redundant fields
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

  auto setCameraControl = std::make_shared<DescriptorSetLayout>(state->getDevice());
  setCameraControl->createUniformBuffer(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);

  auto setCameraEvaluation = std::make_shared<DescriptorSetLayout>(state->getDevice());
  setCameraEvaluation->createUniformBuffer(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

  auto setCameraGeometry = std::make_shared<DescriptorSetLayout>(state->getDevice());
  setCameraGeometry->createUniformBuffer(VK_SHADER_STAGE_GEOMETRY_BIT);

  auto setHeight = std::make_shared<DescriptorSetLayout>(state->getDevice());
  setHeight->createTexture(1, 0, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

  // layout for Color
  _descriptorSetLayout[MaterialType::COLOR].push_back({std::string("cameraControl"), setCameraControl});
  _descriptorSetLayout[MaterialType::COLOR].push_back({std::string("cameraEvaluation"), setCameraEvaluation});
  _descriptorSetLayout[MaterialType::COLOR].push_back(std::pair{std::string("height"), setHeight});
  _descriptorSetLayout[MaterialType::COLOR].push_back(
      {std::string("lightVP"), _lightManager->getDSLViewProjection(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)});
  _descriptorSetLayout[MaterialType::COLOR].push_back(
      {std::string("texture"), _defaultMaterialColor->getDescriptorSetLayoutTextures()});
  _descriptorSetLayout[MaterialType::COLOR].push_back(
      {std::string("shadowTexture"), _lightManager->getDSLShadowTexture()});

  // layout for Phong
  _descriptorSetLayout[MaterialType::PHONG].push_back({std::string("cameraControl"), setCameraControl});
  _descriptorSetLayout[MaterialType::PHONG].push_back({std::string("cameraEvaluation"), setCameraEvaluation});
  _descriptorSetLayout[MaterialType::PHONG].push_back(std::pair{std::string("height"), setHeight});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {std::string("lightVP"), _lightManager->getDSLViewProjection(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {std::string("texture"), _defaultMaterialPhong->getDescriptorSetLayoutTextures()});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {std::string("shadowTexture"), _lightManager->getDSLShadowTexture()});
  _descriptorSetLayout[MaterialType::PHONG].push_back({std::string("lightPhong"), lightManager->getDSLLightPhong()});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"materialCoefficients", _defaultMaterialPhong->getDescriptorSetLayoutCoefficients()});

  // layout for PBR
  _descriptorSetLayout[MaterialType::PBR].push_back({std::string("cameraControl"), setCameraControl});
  _descriptorSetLayout[MaterialType::PBR].push_back({std::string("cameraEvaluation"), setCameraEvaluation});
  _descriptorSetLayout[MaterialType::PBR].push_back(std::pair{std::string("height"), setHeight});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {std::string("lightVP"), _lightManager->getDSLViewProjection(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {std::string("texture"), _defaultMaterialPBR->getDescriptorSetLayoutTextures()});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {std::string("shadowTexture"), _lightManager->getDSLShadowTexture()});
  _descriptorSetLayout[MaterialType::PBR].push_back({std::string("lightPBR"), lightManager->getDSLLightPBR()});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {"materialCoefficients", _defaultMaterialPBR->getDescriptorSetLayoutCoefficients()});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {"alphaMask", _defaultMaterialPBR->getDescriptorSetLayoutAlphaCutoff()});

  // layout for Normal
  _descriptorSetLayoutNormalsMesh.push_back({std::string("cameraControl"), setCameraControl});
  _descriptorSetLayoutNormalsMesh.push_back({std::string("cameraEvaluation"), setCameraEvaluation});
  _descriptorSetLayoutNormalsMesh.push_back({std::string("height"), setHeight});
  _descriptorSetLayoutNormalsMesh.push_back({std::string("cameraGeometry"), setCameraGeometry});

  // layout for Shadows
  _descriptorSetLayoutShadows.push_back({std::string("cameraControl"), setCameraControl});
  _descriptorSetLayoutShadows.push_back({std::string("cameraEvaluation"), setCameraEvaluation});
  _descriptorSetLayoutShadows.push_back({std::string("height"), setHeight});

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

  _descriptorSetCameraControl = std::make_shared<DescriptorSet>(
      state->getSettings()->getMaxFramesInFlight(), setCameraControl, state->getDescriptorPool(), state->getDevice());
  _descriptorSetCameraControl->createUniformBuffer(_cameraBuffer);

  _descriptorSetCameraEvaluation = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                                   setCameraEvaluation, state->getDescriptorPool(),
                                                                   state->getDevice());
  _descriptorSetCameraEvaluation->createUniformBuffer(_cameraBuffer);

  _descriptorSetCameraGeometry = std::make_shared<DescriptorSet>(
      state->getSettings()->getMaxFramesInFlight(), setCameraGeometry, state->getDescriptorPool(), state->getDevice());
  _descriptorSetCameraGeometry->createUniformBuffer(_cameraBuffer);

  _descriptorSetHeight = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(), setHeight,
                                                         state->getDescriptorPool(), state->getDevice());
  _descriptorSetHeight->createTexture({_heightMap});

  for (int i = 0; i < _state->getSettings()->getMaxDirectionalLights(); i++) {
    {
      auto cameraSet = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(), setCameraControl,
                                                       _state->getDescriptorPool(), _state->getDevice());
      cameraSet->createUniformBuffer(_cameraBufferDepth[i][0]);

      _descriptorSetCameraDepthControl.push_back({cameraSet});
    }
    {
      auto cameraSet = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                       setCameraEvaluation, _state->getDescriptorPool(),
                                                       _state->getDevice());
      cameraSet->createUniformBuffer(_cameraBufferDepth[i][0]);

      _descriptorSetCameraDepthEvaluation.push_back({cameraSet});
    }
  }

  for (int i = 0; i < _state->getSettings()->getMaxPointLights(); i++) {
    std::vector<std::shared_ptr<DescriptorSet>> facesSetControl(6);
    std::vector<std::shared_ptr<DescriptorSet>> facesSetEvaluation(6);
    for (int j = 0; j < 6; j++) {
      facesSetControl[j] = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                           setCameraControl, _state->getDescriptorPool(),
                                                           _state->getDevice());
      facesSetControl[j]->createUniformBuffer(
          _cameraBufferDepth[i + _state->getSettings()->getMaxDirectionalLights()][j]);

      facesSetEvaluation[j] = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                              setCameraEvaluation, _state->getDescriptorPool(),
                                                              _state->getDevice());
      facesSetEvaluation[j]->createUniformBuffer(
          _cameraBufferDepth[i + _state->getSettings()->getMaxDirectionalLights()][j]);
    }
    _descriptorSetCameraDepthControl.push_back(facesSetControl);
    _descriptorSetCameraDepthEvaluation.push_back(facesSetEvaluation);
  }

  std::map<std::string, VkPushConstantRange> defaultPushConstants;
  defaultPushConstants["control"] = LoDConstants::getPushConstant();
  defaultPushConstants["evaluate"] = PatchConstants::getPushConstant();
  defaultPushConstants["fragment"] = HeightLevels::getPushConstant();

  // initialize Color
  {
    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add("shaders/terrain/terrainColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/terrain/terrainColor_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    shader->add("shaders/terrain/terrainColor_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    shader->add("shaders/terrain/terrainColor_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    _pipeline[MaterialType::COLOR] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipeline[MaterialType::COLOR]->createGraphicTerrain(
        renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
        _descriptorSetLayout[MaterialType::COLOR], defaultPushConstants, _mesh->getBindingDescription(),
        _mesh->getAttributeDescriptions());

    _pipelineWireframe[MaterialType::COLOR] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineWireframe[MaterialType::COLOR]->createGraphicTerrain(
        renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
        _descriptorSetLayout[MaterialType::COLOR], defaultPushConstants, _mesh->getBindingDescription(),
        _mesh->getAttributeDescriptions());
  }

  // initialize Phong
  {
    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add("shaders/terrain/terrainColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/terrain/terrainPhong_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    shader->add("shaders/terrain/terrainColor_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    shader->add("shaders/terrain/terrainColor_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    _pipeline[MaterialType::PHONG] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipeline[MaterialType::PHONG]->createGraphicTerrain(
        renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
        _descriptorSetLayout[MaterialType::PHONG], defaultPushConstants, _mesh->getBindingDescription(),
        _mesh->getAttributeDescriptions());

    _pipelineWireframe[MaterialType::PHONG] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineWireframe[MaterialType::PHONG]->createGraphicTerrain(
        renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
        _descriptorSetLayout[MaterialType::PHONG], defaultPushConstants, _mesh->getBindingDescription(),
        _mesh->getAttributeDescriptions());
  }

  // initialize PBR
  {
    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add("shaders/terrain/terrainColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/terrain/terrainPBR_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    shader->add("shaders/terrain/terrainColor_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    shader->add("shaders/terrain/terrainColor_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    _pipeline[MaterialType::PBR] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipeline[MaterialType::PBR]->createGraphicTerrain(
        renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
        _descriptorSetLayout[MaterialType::PBR], defaultPushConstants, _mesh->getBindingDescription(),
        _mesh->getAttributeDescriptions());

    _pipelineWireframe[MaterialType::PBR] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineWireframe[MaterialType::PBR]->createGraphicTerrain(
        renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
        _descriptorSetLayout[MaterialType::PBR], defaultPushConstants, _mesh->getBindingDescription(),
        _mesh->getAttributeDescriptions());
  }

  // initialize Normal (per vertex)
  {
    auto shaderNormal = std::make_shared<Shader>(state->getDevice());
    shaderNormal->add("shaders/terrain/terrain_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderNormal->add("shaders/terrain/terrainNormal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    shaderNormal->add("shaders/terrain/terrain_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    shaderNormal->add("shaders/terrain/terrainNormal_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    shaderNormal->add("shaders/terrain/terrainNormal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

    _pipelineNormalMesh = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineNormalMesh->createGraphicTerrain(
        renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
        {shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
         shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
         shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
         shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
        _descriptorSetLayoutNormalsMesh, defaultPushConstants, _mesh->getBindingDescription(),
        _mesh->getAttributeDescriptions());
  }

  // initialize Tangent (per vertex)
  {
    auto shaderNormal = std::make_shared<Shader>(state->getDevice());
    shaderNormal->add("shaders/terrain/terrain_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderNormal->add("shaders/terrain/terrainNormal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    shaderNormal->add("shaders/terrain/terrain_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    shaderNormal->add("shaders/terrain/terrainTangent_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    shaderNormal->add("shaders/terrain/terrainNormal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

    _pipelineTangentMesh = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineTangentMesh->createGraphicTerrain(
        renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
        {shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
         shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
         shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
         shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
        _descriptorSetLayoutNormalsMesh, defaultPushConstants, _mesh->getBindingDescription(),
        _mesh->getAttributeDescriptions());
  }

  // initialize Shadows (Directional)
  {
    auto shader = std::make_shared<Shader>(state->getDevice());
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
        _mesh->getAttributeDescriptions());
  }

  // initialize Shadows (Point)
  {
    auto shader = std::make_shared<Shader>(state->getDevice());
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
        _mesh->getAttributeDescriptions());
  }
}

void Terrain::enableShadow(bool enable) { _enableShadow = enable; }

void Terrain::enableLighting(bool enable) { _enableLighting = enable; }

void Terrain::setMaterial(std::shared_ptr<MaterialColor> material) {
  _material = material;
  _materialType = MaterialType::COLOR;
}

void Terrain::setMaterial(std::shared_ptr<MaterialPhong> material) {
  _material = material;
  _materialType = MaterialType::PHONG;
}

void Terrain::setMaterial(std::shared_ptr<MaterialPBR> material) {
  _material = material;
  _materialType = MaterialType::PBR;
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

    void* data;
    vkMapMemory(_state->getDevice()->getLogicalDevice(), _cameraBuffer->getBuffer()[currentFrame]->getMemory(), 0,
                sizeof(cameraUBO), 0, &data);
    memcpy(data, &cameraUBO, sizeof(cameraUBO));
    vkUnmapMemory(_state->getDevice()->getLogicalDevice(), _cameraBuffer->getBuffer()[currentFrame]->getMemory());

    VkBuffer vertexBuffers[] = {_mesh->getVertexBuffer()->getBuffer()->getData()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

    auto pipelineLayout = pipeline->getDescriptorSetLayout();
    {
      auto cameralLayoutControl = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                               [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                                 return info.first == std::string("cameraControl");
                                               });
      if (cameralLayoutControl != pipelineLayout.end()) {
        vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->getPipelineLayout(), 0, 1,
                                &_descriptorSetCameraControl->getDescriptorSets()[currentFrame], 0, nullptr);
      }
    }

    {
      auto cameraLayoutEvaluation = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                                 [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                                   return info.first == std::string("cameraEvaluation");
                                                 });
      if (cameraLayoutEvaluation != pipelineLayout.end()) {
        vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->getPipelineLayout(), 1, 1,
                                &_descriptorSetCameraEvaluation->getDescriptorSets()[currentFrame], 0, nullptr);
      }
    }

    {
      auto cameraLayoutGeometry = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                               [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                                 return info.first == std::string("cameraGeometry");
                                               });
      if (cameraLayoutGeometry != pipelineLayout.end()) {
        vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->getPipelineLayout(), 3, 1,
                                &_descriptorSetCameraGeometry->getDescriptorSets()[currentFrame], 0, nullptr);
      }
    }

    {
      auto heightLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                       [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                         return info.first == std::string("height");
                                       });
      if (heightLayout != pipelineLayout.end()) {
        vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->getPipelineLayout(), 2, 1,
                                &_descriptorSetHeight->getDescriptorSets()[currentFrame], 0, nullptr);
      }
    }

    {
      auto terrainLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                          return info.first == std::string("texture");
                                        });
      if (terrainLayout != pipelineLayout.end()) {
        vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->getPipelineLayout(), 4, 1,
                                &_material->getDescriptorSetTextures(currentFrame)->getDescriptorSets()[currentFrame],
                                0, nullptr);
      }
    }

    auto lightLayout = std::find_if(
        pipelineLayout.begin(), pipelineLayout.end(),
        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "lightPhong"; });
    if (lightLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 6, 1,
                              &_lightManager->getDSLightPhong()->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    lightLayout = std::find_if(
        pipelineLayout.begin(), pipelineLayout.end(),
        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "lightPBR"; });
    if (lightLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 6, 1,
                              &_lightManager->getDSLightPBR()->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    auto shadowTextureLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                            [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                              return info.first == "shadowTexture";
                                            });
    if (shadowTextureLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 5, 1,
                              &_lightManager->getDSShadowTexture()[currentFrame]->getDescriptorSets()[currentFrame], 0,
                              nullptr);
    }

    auto lightVPLayout = std::find_if(
        pipelineLayout.begin(), pipelineLayout.end(),
        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "lightVP"; });
    if (lightVPLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 3, 1,
                              &_lightManager->getDSViewProjection(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
                                   ->getDescriptorSets()[currentFrame],
                              0, nullptr);
    }

    auto materialCoefficientsLayout = std::find_if(
        pipelineLayout.begin(), pipelineLayout.end(),
        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
          return info.first == std::string("materialCoefficients");
        });
    // assign coefficients
    if (materialCoefficientsLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 7, 1,
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
                              pipeline->getPipelineLayout(), 8, 1,
                              &_material->getDescriptorSetAlphaCutoff()->getDescriptorSets()[currentFrame], 0, nullptr);
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

  void* data;
  vkMapMemory(_state->getDevice()->getLogicalDevice(),
              _cameraBufferDepth[lightIndexTotal][face]->getBuffer()[currentFrame]->getMemory(), 0, sizeof(cameraUBO),
              0, &data);
  memcpy(data, &cameraUBO, sizeof(cameraUBO));
  vkUnmapMemory(_state->getDevice()->getLogicalDevice(),
                _cameraBufferDepth[lightIndexTotal][face]->getBuffer()[currentFrame]->getMemory());

  VkBuffer vertexBuffers[] = {_mesh->getVertexBuffer()->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  auto pipelineLayout = pipeline->getDescriptorSetLayout();
  auto cameralLayoutControl = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                           [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                             return info.first == std::string("cameraControl");
                                           });
  if (cameralLayoutControl != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(),
        0, 1, &_descriptorSetCameraDepthControl[lightIndexTotal][face]->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  auto cameraLayoutEvaluation = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                             [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                               return info.first == std::string("cameraEvaluation");
                                             });
  if (cameraLayoutEvaluation != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(),
        1, 1, &_descriptorSetCameraDepthEvaluation[lightIndexTotal][face]->getDescriptorSets()[currentFrame], 0,
        nullptr);
  }

  auto heightLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                   [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                     return info.first == std::string("height");
                                   });
  if (heightLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->getPipelineLayout(), 2, 1,
                            &_descriptorSetHeight->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  vkCmdDraw(commandBuffer->getCommandBuffer()[currentFrame], _mesh->getVertexData().size(), 1, 0, 0);
}
}  // namespace VulkanEngine