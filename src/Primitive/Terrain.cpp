#include "Terrain.h"

void Terrain::setModel(glm::mat4 model) { _model = model; }

void Terrain::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

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

TerrainGPU::TerrainGPU(std::array<std::string, 4> tiles,
                       std::string heightMap,
                       std::pair<int, int> patchNumber,
                       std::vector<VkFormat> renderFormat,
                       std::shared_ptr<CommandBuffer> commandBufferTransfer,
                       std::shared_ptr<LightManager> lightManager,
                       std::shared_ptr<State> state) {
  _state = state;
  _patchNumber = patchNumber;
  _lightManager = lightManager;

  _defaultMaterial = std::make_shared<MaterialPhong>(commandBufferTransfer, state);

  _terrainTiles[0] = std::make_shared<Texture>(tiles[0], _state->getSettings()->getLoadTextureColorFormat(),
                                               VK_SAMPLER_ADDRESS_MODE_REPEAT, _mipMap, commandBufferTransfer, state);
  _terrainTiles[1] = std::make_shared<Texture>(tiles[1], _state->getSettings()->getLoadTextureColorFormat(),
                                               VK_SAMPLER_ADDRESS_MODE_REPEAT, _mipMap, commandBufferTransfer, state);
  _terrainTiles[2] = std::make_shared<Texture>(tiles[2], _state->getSettings()->getLoadTextureColorFormat(),
                                               VK_SAMPLER_ADDRESS_MODE_REPEAT, _mipMap, commandBufferTransfer, state);
  _terrainTiles[3] = std::make_shared<Texture>(tiles[3], _state->getSettings()->getLoadTextureColorFormat(),
                                               VK_SAMPLER_ADDRESS_MODE_REPEAT, _mipMap, commandBufferTransfer, state);

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

  _cameraBuffer = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(BufferMVP),
                                                  state->getDevice());

  for (int i = 0; i < state->getSettings()->getMaxDirectionalLights(); i++) {
    _cameraBufferDepth.push_back({std::make_shared<UniformBuffer>(state->getSettings()->getMaxFramesInFlight(),
                                                                  sizeof(BufferMVP), state->getDevice())});
  }

  for (int i = 0; i < state->getSettings()->getMaxPointLights(); i++) {
    std::vector<std::shared_ptr<UniformBuffer>> facesBuffer(6);
    for (int j = 0; j < 6; j++) {
      facesBuffer[j] = std::make_shared<UniformBuffer>(state->getSettings()->getMaxFramesInFlight(), sizeof(BufferMVP),
                                                       state->getDevice());
    }
    _cameraBufferDepth.push_back(facesBuffer);
  }

  auto setCameraControl = std::make_shared<DescriptorSetLayout>(state->getDevice());
  setCameraControl->createUniformBuffer(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);

  auto setCameraEvaluation = std::make_shared<DescriptorSetLayout>(state->getDevice());
  setCameraEvaluation->createUniformBuffer(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

  auto setCameraGeometry = std::make_shared<DescriptorSetLayout>(state->getDevice());
  setCameraGeometry->createUniformBuffer(VK_SHADER_STAGE_GEOMETRY_BIT);

  auto setHeight = std::make_shared<DescriptorSetLayout>(state->getDevice());
  setHeight->createTexture(1, 0, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

  auto setTerrainTiles = std::make_shared<DescriptorSetLayout>(state->getDevice());
  setTerrainTiles->createTexture(4);

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

  _descriptorSetTerrainTiles = std::make_shared<DescriptorSet>(
      state->getSettings()->getMaxFramesInFlight(), setTerrainTiles, state->getDescriptorPool(), state->getDevice());
  _descriptorSetTerrainTiles->createTexture(
      std::vector<std::shared_ptr<Texture>>(_terrainTiles.begin(), _terrainTiles.end()));

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

  auto shader = std::make_shared<Shader>(state->getDevice());
  shader->add("shaders/terrainGPU_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader->add("shaders/terrainGPU_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  shader->add("shaders/terrainGPU_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
  shader->add("shaders/terrainGPU_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

  auto shaderNormal = std::make_shared<Shader>(state->getDevice());
  shaderNormal->add("shaders/terrainGPU_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shaderNormal->add("shaders/normal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  shaderNormal->add("shaders/terrainGPU_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
  shaderNormal->add("shaders/normal_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
  shaderNormal->add("shaders/normal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

  std::map<std::string, VkPushConstantRange> defaultPushConstants;
  defaultPushConstants["control"] = LoDConstants::getPushConstant();
  defaultPushConstants["evaluate"] = PatchConstants::getPushConstant();
  defaultPushConstants["fragment"] = HeightLevels::getPushConstant();

  _pipeline[TerrainRenderMode::FULL] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _pipeline[TerrainRenderMode::FULL]->createGraphicTerrainGPU(
      renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
      {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT), shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
       shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
       shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
      {std::pair{std::string("cameraControl"), setCameraControl},
       std::pair{std::string("cameraEvaluation"), setCameraEvaluation}, std::pair{std::string("height"), setHeight},
       std::pair{std::string("terrainTiles"), setTerrainTiles},
       std::pair{std::string("light"), lightManager->getDSLLight()},
       std::pair{std::string("shadowTexture"), _lightManager->getDSLShadowTexture()},
       std::pair{std::string("lightVP"),
                 _lightManager->getDSLViewProjection(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
       std::pair{"phongCoefficients", _defaultMaterial->getDescriptorSetLayoutCoefficients()}},
      defaultPushConstants, _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());

  _pipelineNormal = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _pipelineNormal->createGraphicTerrainGPU(
      renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
      {shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
       shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
       shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
       shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
       shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
      {std::pair{std::string("cameraControl"), setCameraControl},
       std::pair{std::string("cameraEvaluation"), setCameraEvaluation}, std::pair{std::string("height"), setHeight},
       std::pair{std::string("cameraGeometry"), setCameraGeometry}},
      defaultPushConstants, _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());

  _pipelineWireframe = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _pipelineWireframe->createGraphicTerrainGPU(
      renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
      {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT), shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
       shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
       shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
      {std::pair{std::string("cameraControl"), setCameraControl},
       std::pair{std::string("cameraEvaluation"), setCameraEvaluation}, std::pair{std::string("height"), setHeight},
       std::pair{std::string("terrainTiles"), setTerrainTiles},
       std::pair{std::string("light"), lightManager->getDSLLight()},
       std::pair{std::string("shadowTexture"), _lightManager->getDSLShadowTexture()},
       std::pair{std::string("lightVP"),
                 _lightManager->getDSLViewProjection(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
       std::pair{"phongCoefficients", _defaultMaterial->getDescriptorSetLayoutCoefficients()}},
      defaultPushConstants, _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());

  {
    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add("shaders/terrainDepthGPU_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/terrainDepthGPU_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    shader->add("shaders/terrainDepthGPU_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["control"] = LoDConstants::getPushConstant();
    defaultPushConstants["evaluate"] = PatchConstants::getPushConstant();

    _pipeline[TerrainRenderMode::DIRECTIONAL] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipeline[TerrainRenderMode::DIRECTIONAL]->createGraphicTerrainShadowGPU(
        VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
        {std::pair{std::string("cameraControl"), setCameraControl},
         std::pair{std::string("cameraEvaluation"), setCameraEvaluation}, std::pair{std::string("height"), setHeight}},
        defaultPushConstants, _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }
  {
    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add("shaders/terrainDepthGPU_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/terrainDepthGPU_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    shader->add("shaders/terrainDepthGPU_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    shader->add("shaders/terrainDepthGPU_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["control"] = LoDConstants::getPushConstant();
    defaultPushConstants["evaluate"] = PatchConstants::getPushConstant();
    defaultPushConstants["fragment"] = DepthConstants::getPushConstant(sizeof(LoDConstants) + sizeof(PatchConstants));

    _pipeline[TerrainRenderMode::POINT] = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipeline[TerrainRenderMode::POINT]->createGraphicTerrainShadowGPU(
        VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        {std::pair{std::string("cameraControl"), setCameraControl},
         std::pair{std::string("cameraEvaluation"), setCameraEvaluation}, std::pair{std::string("height"), setHeight}},
        defaultPushConstants, _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }
}

void TerrainGPU::showLoD(bool enable) { _showLoD = enable; }

void TerrainGPU::patchEdge(bool enable) { _enableEdge = enable; }

void TerrainGPU::draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer, DrawType pipelineType) {
  auto drawTerrain = [&](std::shared_ptr<Pipeline> pipeline) {
    vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline->getPipeline());
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
      pushConstants.cameraPosition = _camera->getEye();
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(LoDConstants) + sizeof(PatchConstants),
                         sizeof(HeightLevels), &pushConstants);
    }

    // same buffer to both tessellation shaders because we're not going to change camera between these 2 stages
    BufferMVP cameraUBO{};
    cameraUBO.model = _model;
    cameraUBO.view = _camera->getView();
    cameraUBO.projection = _camera->getProjection();

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
                                          return info.first == std::string("terrainTiles");
                                        });
      if (terrainLayout != pipelineLayout.end()) {
        vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->getPipelineLayout(), 3, 1,
                                &_descriptorSetTerrainTiles->getDescriptorSets()[currentFrame], 0, nullptr);
      }
    }

    auto lightLayout = std::find_if(
        pipelineLayout.begin(), pipelineLayout.end(),
        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "light"; });
    if (lightLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 4, 1,
                              &_lightManager->getDSLight()->getDescriptorSets()[currentFrame], 0, nullptr);
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
                              pipeline->getPipelineLayout(), 6, 1,
                              &_lightManager->getDSViewProjection(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
                                   ->getDescriptorSets()[currentFrame],
                              0, nullptr);
    }

    auto phongCoefficientsLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                                [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                                  return info.first == std::string("phongCoefficients");
                                                });
    // assign coefficients
    if (phongCoefficientsLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(
          commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
          pipeline->getPipelineLayout(), 7, 1,
          &_defaultMaterial->getDescriptorSetCoefficients(currentFrame)->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    vkCmdDraw(commandBuffer->getCommandBuffer()[currentFrame], _mesh->getVertexData().size(), 1, 0, 0);
  };

  std::shared_ptr<Pipeline> pipeline = _pipeline[TerrainRenderMode::FULL];
  if ((pipelineType & DrawType::WIREFRAME) == DrawType::WIREFRAME) pipeline = _pipelineWireframe;
  drawTerrain(pipeline);

  if ((pipelineType & DrawType::NORMAL) == DrawType::NORMAL) drawTerrain(_pipelineNormal);
}

void TerrainGPU::drawShadow(int currentFrame,
                            std::shared_ptr<CommandBuffer> commandBuffer,
                            LightType lightType,
                            int lightIndex,
                            int face) {
  auto pipeline = _pipeline[(TerrainRenderMode)lightType];

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