#include "Primitive/Terrain.h"
#undef far
#undef near
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>

TerrainPhysics::TerrainPhysics(std::shared_ptr<ImageCPU<uint8_t>> heightmap,
                               glm::vec3 position,
                               glm::vec3 scale,
                               std::tuple<int, int> heightScaleOffset,
                               std::shared_ptr<PhysicsManager> physicsManager) {
  _physicsManager = physicsManager;
  _resolution = heightmap->getResolution();
  auto [w, h] = _resolution;
  int c = heightmap->getChannels();
  for (int i = 0; i < w * h * c; i += c) {
    _terrainPhysic.push_back((float)heightmap->getData()[i] / 255.f);
  }

  // Create height field
  JPH::HeightFieldShapeSettings settingsTerrain(_terrainPhysic.data(),
                                                JPH::Vec3(0.f, -std::get<1>(heightScaleOffset), 0.f),
                                                JPH::Vec3(1.f, std::get<0>(heightScaleOffset), 1.f), w);

  auto heightField = JPH::StaticCast<JPH::HeightFieldShape>(settingsTerrain.Create().Get());
  _heights.resize(w * h);
  heightField->GetHeights(0, 0, w, h, _heights.data(), w);
  _scale = scale;

  JPH::Ref<JPH::RotatedTranslatedShape> rotateTranslatedHeightField = new JPH::RotatedTranslatedShape(
      JPH::Vec3(-w / 2.f, 0.f, -h / 2.f), JPH::Quat::sIdentity(), heightField);
  _terrainShape = new JPH::ScaledShape(rotateTranslatedHeightField, JPH::Vec3(scale.x, scale.y, scale.z));
  // anchor point is top-left corner in physics, but center in graphic
  _position = glm::vec3(position.x, position.y, position.z);
  auto terrainBody = _physicsManager->getBodyInterface().CreateBody(
      JPH::BodyCreationSettings(_terrainShape, JPH::Vec3(_position.x, _position.y, _position.z), JPH::Quat::sIdentity(),
                                JPH::EMotionType::Static, Layers::NON_MOVING));
  _terrainID = terrainBody->GetID();
  _physicsManager->getBodyInterface().AddBody(_terrainID, JPH::EActivation::DontActivate);
}

std::vector<float> TerrainPhysics::getHeights() { return _heights; }

void TerrainPhysics::setPosition(glm::vec3 position) {
  int w = std::get<0>(_resolution);
  _position = glm::vec3(position.x, position.y, position.z);
  _physicsManager->getBodyInterface().SetPosition(_terrainID, JPH::Vec3(_position.x, _position.y, _position.z),
                                                  JPH::EActivation::DontActivate);
}

std::optional<glm::vec3> TerrainPhysics::hit(glm::vec3 origin, glm::vec3 direction) {
  JPH::RRayCast rray{JPH::RVec3(origin.x, origin.y, origin.z), JPH::Vec3(direction.x, direction.y, direction.z)};
  JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
  _physicsManager->getPhysicsSystem().GetNarrowPhaseQuery().CastRay(rray, JPH::RayCastSettings(), collector);
  if (collector.HadHit()) {
    for (auto& hit : collector.mHits) {
      if (hit.mBodyID == _terrainID) {
        auto outPosition = rray.GetPointOnRay(hit.mFraction);
        return glm::vec3(outPosition.GetX(), outPosition.GetY(), outPosition.GetZ());
      }
    }
  }

  return std::nullopt;
}

std::tuple<int, int> TerrainPhysics::getResolution() { return _resolution; }

glm::vec3 TerrainPhysics::getPosition() { return _position; }

TerrainPhysics::~TerrainPhysics() {
  _physicsManager->getBodyInterface().RemoveBody(_terrainID);
  _physicsManager->getBodyInterface().DestroyBody(_terrainID);
}

TerrainCPU::TerrainCPU(std::shared_ptr<ImageCPU<uint8_t>> heightMap,
                       std::pair<int, int> patchNumber,
                       std::shared_ptr<CommandBuffer> commandBufferTransfer,
                       std::shared_ptr<GameState> gameState,
                       std::shared_ptr<EngineState> engineState) {
  setName("TerrainCPU");
  _engineState = engineState;
  _gameState = gameState;
  _patchNumber = patchNumber;

  _loadStrip(heightMap, commandBufferTransfer, engineState);
  _loadTerrain(commandBufferTransfer, engineState);
}

TerrainCPU::TerrainCPU(std::vector<float> heights,
                       std::tuple<int, int> resolution,
                       std::shared_ptr<CommandBuffer> commandBufferTransfer,
                       std::shared_ptr<GameState> gameState,
                       std::shared_ptr<EngineState> engineState) {
  setName("TerrainCPU");
  _engineState = engineState;
  _gameState = gameState;

  _loadTriangles(heights, resolution, commandBufferTransfer, engineState);
  _loadTerrain(commandBufferTransfer, engineState);
}

void TerrainCPU::_updateColorDescriptor() {
  std::vector<VkDescriptorImageInfo> colorImageInfo(_engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
    std::vector<VkDescriptorBufferInfo> bufferInfoCamera(1);
    bufferInfoCamera[0].buffer = _cameraBuffer->getBuffer()[i]->getData();
    bufferInfoCamera[0].offset = 0;
    bufferInfoCamera[0].range = sizeof(BufferMVP);
    bufferInfoColor[0] = bufferInfoCamera;

    _descriptorSetColor->createCustom(i, bufferInfoColor, {});
  }
}

void TerrainCPU::_loadStrip(std::shared_ptr<ImageCPU<uint8_t>> heightMap,
                            std::shared_ptr<CommandBuffer> commandBufferTransfer,
                            std::shared_ptr<EngineState> engineState) {
  // vertex generation
  std::vector<Vertex3D> vertices;
  std::vector<uint32_t> indices;
  _mesh = std::make_shared<MeshStatic3D>(engineState);
  auto [width, height] = heightMap->getResolution();
  auto channels = heightMap->getChannels();
  auto data = heightMap->getData();
  // 255 - max value from height map, 256 is number of colors
  for (unsigned int i = 0; i < height; i++) {
    for (unsigned int j = 0; j < width; j++) {
      // retrieve texel for (i,j) tex coord
      auto y = data[(j + width * i) * channels];

      // vertex
      Vertex3D vertex;
      vertex.pos = glm::vec3(-width / 2.0f + j, (int)y * _heightScale / 256.f - _heightShift, -height / 2.0f + i);
      vertices.push_back(vertex);
    }
  }

  // TODO: refactor to memcpy? Why it uses GPU at all, read vulkan-tutorial
  _mesh->setVertices(vertices, commandBufferTransfer);

  // index generation
  for (unsigned int i = 0; i < height - 1; i++) {  // for each row a.k.a. each strip
    for (unsigned int j = 0; j < width; j++) {     // for each column
      for (unsigned int k = 0; k < 2; k++) {       // for each side of the strip
        indices.push_back(j + width * (i + k));
      }
    }
  }

  _numStrips = height - 1;
  _numVertsPerStrip = width * 2;

  _mesh->setIndexes(indices, commandBufferTransfer);

  _hasIndexes = true;
}

void TerrainCPU::_loadTriangles(std::vector<float> heights,
                                std::tuple<int, int> resolution,
                                std::shared_ptr<CommandBuffer> commandBufferTransfer,
                                std::shared_ptr<EngineState> engineState) {
  auto [width, height] = resolution;
  std::vector<Vertex3D> vertices;
  _mesh = std::make_shared<MeshStatic3D>(engineState);
  for (int y = 0; y < height - 1; y++) {
    for (int x = 0; x < width - 1; x++) {
      // define patch: 4 points (square)
      Vertex3D vertex1{};
      vertex1.pos = glm::vec3(-width / 2.0f + x, heights[x + width * y], -height / 2.0f + y);
      vertices.push_back(vertex1);

      Vertex3D vertex2{};
      vertex2.pos = glm::vec3(-width / 2.0f + (x + 1), heights[x + 1 + width * y], -height / 2.0f + y);
      vertices.push_back(vertex2);

      Vertex3D vertex3{};
      vertex3.pos = glm::vec3(-width / 2.0f + x, heights[x + width * (y + 1)], -height / 2.0f + (y + 1));
      vertices.push_back(vertex3);

      vertices.push_back(vertex3);
      vertices.push_back(vertex2);

      Vertex3D vertex4{};
      vertex4.pos = glm::vec3(-width / 2.0f + (x + 1), heights[x + 1 + width * (y + 1)], -height / 2.0f + (y + 1));
      vertices.push_back(vertex4);
    }
  }
  _mesh->setVertices(vertices, commandBufferTransfer);

  _numStrips = height - 1;
  _numVertsPerStrip = (width - 1) * 6;
}

void TerrainCPU::_loadTerrain(std::shared_ptr<CommandBuffer> commandBufferTransfer,
                              std::shared_ptr<EngineState> engineState) {
  _renderPass = std::make_shared<RenderPass>(_engineState->getSettings(), _engineState->getDevice());
  _renderPass->initializeGraphic();

  _cameraBuffer = std::make_shared<UniformBuffer>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                  sizeof(BufferMVP), engineState);

  // layout for Color
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutColor(1);
    layoutColor[0].binding = 0;
    layoutColor[0].descriptorCount = 1;
    layoutColor[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutColor[0].pImmutableSamplers = nullptr;
    layoutColor[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    descriptorSetLayout->createCustom(layoutColor);
    _descriptorSetLayout.push_back({"color", descriptorSetLayout});
    _descriptorSetColor = std::make_shared<DescriptorSet>(engineState->getSettings()->getMaxFramesInFlight(),
                                                          descriptorSetLayout, engineState->getDescriptorPool(),
                                                          engineState->getDevice());
    // update descriptor set in setMaterial
    _updateColorDescriptor();

    // initialize Color
    {
      auto shader = std::make_shared<Shader>(engineState);
      shader->add("shaders/terrain/terrainCPU_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/terrain/terrainCPU_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      _pipeline = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());
      _pipeline->createGeometry(
          VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          _descriptorSetLayout, {}, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)}}), _renderPass);

      _pipelineWireframe = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());
      _pipelineWireframe->createGeometry(
          VK_CULL_MODE_NONE, VK_POLYGON_MODE_LINE, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          _descriptorSetLayout, {}, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)}}), _renderPass);
    }
  }
}

void TerrainCPU::setDrawType(DrawType drawType) { _drawType = drawType; }

DrawType TerrainCPU::getDrawType() { return _drawType; }

void TerrainCPU::patchEdge(bool enable) { _enableEdge = enable; }

void TerrainCPU::draw(std::shared_ptr<CommandBuffer> commandBuffer) {
  int currentFrame = _engineState->getFrameInFlight();
  auto drawTerrain = [&](std::shared_ptr<Pipeline> pipeline) {
    vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline->getPipeline());

    auto resolution = _engineState->getSettings()->getResolution();
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

    // same buffer to both tessellation shaders because we're not going to change camera between these 2 stages
    BufferMVP cameraUBO{};
    cameraUBO.model = _model;
    cameraUBO.view = _gameState->getCameraManager()->getCurrentCamera()->getView();
    cameraUBO.projection = _gameState->getCameraManager()->getCurrentCamera()->getProjection();

    _cameraBuffer->getBuffer()[currentFrame]->setData(&cameraUBO);

    VkBuffer vertexBuffers[] = {_mesh->getVertexBuffer()->getBuffer()->getData()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);
    if (_hasIndexes) {
      VkBuffer indexBuffers = _mesh->getIndexBuffer()->getBuffer()->getData();
      vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame], indexBuffers, 0, VK_INDEX_TYPE_UINT32);
    }

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

    if (_hasIndexes) {
      for (unsigned int strip = 0; strip < _numStrips; ++strip) {
        vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], _numVertsPerStrip, 1,
                         strip * _numVertsPerStrip, 0, 0);
      }
    } else {
      for (int i = 0; i < _numStrips; i++)
        vkCmdDraw(commandBuffer->getCommandBuffer()[currentFrame], _numVertsPerStrip, 1, i * _numVertsPerStrip, 0);
    }
  };

  auto pipeline = _pipeline;
  if (_drawType == DrawType::WIREFRAME) pipeline = _pipelineWireframe;
  drawTerrain(pipeline);
}

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

struct HeightLevelsDebug {
  alignas(16) float heightLevels[4];
  alignas(16) int patchEdge;
  alignas(16) int showLOD;
  alignas(16) int enableShadow;
  alignas(16) int enableLighting;
  alignas(16) glm::vec3 cameraPosition;
  alignas(16) int tile;
  static VkPushConstantRange getPushConstant() {
    VkPushConstantRange pushConstant{};
    // this push constant range starts at the beginning
    // this push constant range takes up the size of a MeshPushConstants struct
    pushConstant.offset = sizeof(LoDConstants) + sizeof(PatchConstants);
    pushConstant.size = sizeof(HeightLevelsDebug);
    // this push constant range is accessible only in the vertex shader
    pushConstant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    return pushConstant;
  }
};

struct HeightLevels {
  alignas(16) float heightLevels[4];
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

TerrainDebug::TerrainDebug(std::shared_ptr<ImageCPU<uint8_t>> heightMapCPU,
                           std::pair<int, int> patchNumber,
                           std::shared_ptr<CommandBuffer> commandBufferTransfer,
                           std::shared_ptr<GUI> gui,
                           std::shared_ptr<GameState> gameState,
                           std::shared_ptr<EngineState> engineState) {
  setName("Terrain");
  _engineState = engineState;
  _gameState = gameState;
  _patchNumber = patchNumber;
  _gui = gui;

  // needed for layout
  _defaultMaterialColor = std::make_shared<MaterialColor>(MaterialTarget::TERRAIN, commandBufferTransfer, engineState);
  _defaultMaterialColor->setBaseColor(std::vector{4, _gameState->getResourceManager()->getTextureOne()});
  _material = _defaultMaterialColor;
  _heightMapCPU = heightMapCPU;
  _heightMapGPU = _gameState->getResourceManager()->loadImageGPU<uint8_t>({heightMapCPU});
  _changedHeightmap.resize(_engineState->getSettings()->getMaxFramesInFlight());
  _heightMap = std::make_shared<Texture>(_heightMapGPU, _engineState->getSettings()->getLoadTextureAuxilaryFormat(),
                                         VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, VK_FILTER_LINEAR, commandBufferTransfer,
                                         engineState);

  _changeMesh.resize(engineState->getSettings()->getMaxFramesInFlight());
  _mesh.resize(engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _mesh[i] = std::make_shared<MeshDynamic3D>(engineState);
    _calculateMesh(i);
  }

  std::map<std::string, VkPushConstantRange> defaultPushConstants;
  defaultPushConstants["control"] = LoDConstants::getPushConstant();
  defaultPushConstants["evaluate"] = PatchConstants::getPushConstant();
  defaultPushConstants["fragment"] = HeightLevelsDebug::getPushConstant();

  _renderPass = std::make_shared<RenderPass>(_engineState->getSettings(), _engineState->getDevice());
  _renderPass->initializeGraphic();
  _cameraBuffer = std::make_shared<UniformBuffer>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                  sizeof(BufferMVP), engineState);

  // layout for Normal
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
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
    _descriptorSetNormal = std::make_shared<DescriptorSet>(engineState->getSettings()->getMaxFramesInFlight(),
                                                           descriptorSetLayout, engineState->getDescriptorPool(),
                                                           engineState->getDevice());
    for (int i = 0; i < engineState->getSettings()->getMaxFramesInFlight(); i++) {
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
      auto shaderNormal = std::make_shared<Shader>(engineState);
      shaderNormal->add("shaders/terrain/terrainDebug_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shaderNormal->add("shaders/terrain/terrainNormal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shaderNormal->add("shaders/terrain/terrainDebug_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shaderNormal->add("shaders/terrain/terrainNormal_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      shaderNormal->add("shaders/terrain/terrainNormal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

      _pipelineNormalMesh = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());
      _pipelineNormalMesh->createGraphicTerrain(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
          {shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
          _descriptorSetLayoutNormalsMesh, defaultPushConstants, _mesh[0]->getBindingDescription(),
          _mesh[0]->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                    {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPass);
    }

    // initialize Tangent (per vertex)
    {
      auto shaderNormal = std::make_shared<Shader>(engineState);
      shaderNormal->add("shaders/terrain/terrainDebug_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shaderNormal->add("shaders/terrain/terrainNormal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shaderNormal->add("shaders/terrain/terrainDebug_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shaderNormal->add("shaders/terrain/terrainTangent_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      shaderNormal->add("shaders/terrain/terrainNormal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

      _pipelineTangentMesh = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());
      _pipelineTangentMesh->createGraphicTerrain(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
          {shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
          _descriptorSetLayoutNormalsMesh, defaultPushConstants, _mesh[0]->getBindingDescription(),
          _mesh[0]->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                    {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPass);
    }
  }

  _reallocatePatch.resize(engineState->getSettings()->getMaxFramesInFlight());
  _patchDescriptionSSBO.resize(engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _reallocatePatchDescription(i);
  }
  // layout for Color
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutColor(5);
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
    layoutColor[4].binding = 4;
    layoutColor[4].descriptorCount = 1;
    layoutColor[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutColor[4].pImmutableSamplers = nullptr;
    layoutColor[4].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    descriptorSetLayout->createCustom(layoutColor);
    _descriptorSetLayout.push_back({"color", descriptorSetLayout});
    _descriptorSetColor = std::make_shared<DescriptorSet>(engineState->getSettings()->getMaxFramesInFlight(),
                                                          descriptorSetLayout, engineState->getDescriptorPool(),
                                                          engineState->getDevice());
    _updateColorDescriptor({_defaultMaterialColor});

    // initialize Color
    {
      auto shader = std::make_shared<Shader>(engineState);
      shader->add("shaders/terrain/terrainDebug_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/terrain/terrainDebug_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shader->add("shaders/terrain/terrainDebug_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shader->add("shaders/terrain/terrainDebug_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      _pipeline = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());
      _pipeline->createGraphicTerrain(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
          _descriptorSetLayout, defaultPushConstants, _mesh[0]->getBindingDescription(),
          _mesh[0]->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                    {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPass);

      _pipelineWireframe = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());
      _pipelineWireframe->createGraphicTerrain(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
          _descriptorSetLayout, defaultPushConstants, _mesh[0]->getBindingDescription(),
          _mesh[0]->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                    {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPass);
    }
  }

  _changePatch.resize(engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _updatePatchDescription(i);
  }
}

void TerrainDebug::_calculateMesh(int index) {
  auto [width, height] = _heightMap->getImageView()->getImage()->getResolution();
  std::vector<Vertex3D> vertices;

  for (int y = 0; y < _patchNumber.second; y++) {
    for (int x = 0; x < _patchNumber.first; x++) {
      // define patch: 4 points (square)
      Vertex3D vertex1{};
      vertex1.pos = glm::vec3(-width / 2.0f + (width - 1) * x / (float)_patchNumber.first, 0.f,
                              -height / 2.0f + (height - 1) * y / (float)_patchNumber.second);
      vertex1.texCoord = glm::vec2(x, y);
      vertices.push_back(vertex1);

      Vertex3D vertex2{};
      vertex2.pos = glm::vec3(-width / 2.0f + (width - 1) * (x + 1) / (float)_patchNumber.first, 0.f,
                              -height / 2.0f + (height - 1) * y / (float)_patchNumber.second);
      vertex2.texCoord = glm::vec2(x + 1, y);
      vertices.push_back(vertex2);

      Vertex3D vertex3{};
      vertex3.pos = glm::vec3(-width / 2.0f + (width - 1) * x / (float)_patchNumber.first, 0.f,
                              -height / 2.0f + (height - 1) * (y + 1) / (float)_patchNumber.second);
      vertex3.texCoord = glm::vec2(x, y + 1);
      vertices.push_back(vertex3);

      Vertex3D vertex4{};
      vertex4.pos = glm::vec3(-width / 2.0f + (width - 1) * (x + 1) / (float)_patchNumber.first, 0.f,
                              -height / 2.0f + (height - 1) * (y + 1) / (float)_patchNumber.second);
      vertex4.texCoord = glm::vec2(x + 1, y + 1);
      vertices.push_back(vertex4);
    }
  }
  _mesh[index]->setVertices(vertices);
}

void TerrainDebug::_changeHeightmap(glm::ivec2 position, int value) {
  auto [width, height] = _heightMapCPU->getResolution();
  auto data = _heightMapCPU->getData();
  data[(position.x + position.y * width) * _heightMapCPU->getChannels()] += value;
  data[(position.x + position.y * width) * _heightMapCPU->getChannels() + 1] += value;
  data[(position.x + position.y * width) * _heightMapCPU->getChannels() + 2] += value;
  data[(position.x + position.y * width) * _heightMapCPU->getChannels() + 3] += value;
  _heightMapCPU->setData(data);
  _heightMapGPU->setData(_heightMapCPU->getData().get());
}

glm::ivec2 TerrainDebug::_calculatePixelByPosition(glm::vec3 position) {
  auto [width, height] = _heightMap->getImageView()->getImage()->getResolution();

  auto leftTop = _model * glm::vec4(-width / 2.0f, 0.f, -height / 2.0f, 1.f);
  auto rightTop = _model * glm::vec4(-width / 2.0f + (width - 1), 0.f, -height / 2.0f, 1.f);
  auto leftBot = _model * glm::vec4(-width / 2.0f, 0.f, -height / 2.0f + height - 1, 1.f);
  auto rightBot = _model * glm::vec4(-width / 2.0f + (width - 1), 0.f, -height / 2.0f + height - 1, 1.f);
  glm::vec3 terrainSize = rightBot - leftTop;
  glm::vec2 ratio = glm::vec2((glm::vec4(position, 1.f) - leftTop).x / terrainSize.x,
                              (glm::vec4(position, 1.f) - leftTop).z / terrainSize.z);
  auto textureCoords = glm::vec2(ratio.x * width, ratio.y * height);
  return textureCoords;
}

int TerrainDebug::_calculateTileByPosition(glm::vec3 position) {
  auto [width, height] = _heightMap->getImageView()->getImage()->getResolution();
  for (int y = 0; y < _patchNumber.second; y++)
    for (int x = 0; x < _patchNumber.first; x++) {
      // define patch: 4 points (square)
      auto vertex1 = glm::vec3(-width / 2.0f + (width - 1) * x / (float)_patchNumber.first, 0.f,
                               -height / 2.0f + (height - 1) * y / (float)_patchNumber.second);
      vertex1 = _model * glm::vec4(vertex1, 1.f);
      auto vertex2 = glm::vec3(-width / 2.0f + (width - 1) * (x + 1) / (float)_patchNumber.first, 0.f,
                               -height / 2.0f + (height - 1) * y / (float)_patchNumber.second);
      vertex2 = _model * glm::vec4(vertex2, 1.f);
      auto vertex3 = glm::vec3(-width / 2.0f + (width - 1) * x / (float)_patchNumber.first, 0.f,
                               -height / 2.0f + (height - 1) * (y + 1) / (float)_patchNumber.second);
      vertex3 = _model * glm::vec4(vertex3, 1.f);

      if (position.x > vertex1.x && position.x < vertex2.x && position.z > vertex1.z && position.z < vertex3.z)
        return x + y * _patchNumber.first;
    }

  return -1;
}

void TerrainDebug::_updateColorDescriptor(std::shared_ptr<MaterialColor> material) {
  std::vector<VkDescriptorImageInfo> colorImageInfo(_engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
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

    std::vector<VkDescriptorBufferInfo> bufferPatchInfo(1);
    bufferPatchInfo[0].buffer = _patchDescriptionSSBO[i]->getData();
    bufferPatchInfo[0].offset = 0;
    bufferPatchInfo[0].range = _patchDescriptionSSBO[i]->getSize();
    bufferInfoColor[4] = bufferPatchInfo;

    _descriptorSetColor->createCustom(i, bufferInfoColor, textureInfoColor);
  }
  _material->unregisterUpdate(_descriptorSetColor);
  material->registerUpdate(_descriptorSetColor, {{MaterialTexture::COLOR, 3}});
}

void TerrainDebug::_reallocatePatchDescription(int currentFrame) {
  _patchRotations = std::vector<glm::mat4>(_patchNumber.first * _patchNumber.second, glm::mat4(1.f));

  _patchDescriptionSSBO[currentFrame] = std::make_shared<Buffer>(
      _patchNumber.first * _patchNumber.second * sizeof(glm::mat4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);
}

void TerrainDebug::_updatePatchDescription(int currentFrame) {
  // fill only number of lights because it's stub
  _patchDescriptionSSBO[currentFrame]->setData(_patchRotations.data(), _patchRotations.size() * sizeof(glm::mat4));
}

void TerrainDebug::setTerrainPhysics(std::shared_ptr<TerrainPhysics> terrainPhysics) {
  _terrainPhysics = terrainPhysics;
}

void TerrainDebug::setTessellationLevel(int min, int max) {
  _minTessellationLevel = min;
  _maxTessellationLevel = max;
}

void TerrainDebug::setDisplayDistance(int min, int max) {
  _minDistance = min;
  _maxDistance = max;
}

void TerrainDebug::setHeight(float scale, float shift) {
  _heightScale = scale;
  _heightShift = shift;
}

void TerrainDebug::setColorHeightLevels(std::array<float, 4> levels) { _heightLevels = levels; }

void TerrainDebug::setMaterial(std::shared_ptr<MaterialColor> material) {
  _updateColorDescriptor(material);
  _material = material;
}

void TerrainDebug::setDrawType(DrawType drawType) { _drawType = drawType; }

DrawType TerrainDebug::getDrawType() { return _drawType; }

void TerrainDebug::showLoD(bool enable) { _showLoD = enable; }

void TerrainDebug::patchEdge(bool enable) { _enableEdge = enable; }

void TerrainDebug::setTileRotation(int tileID, glm::mat4 rotation) { _patchRotations[tileID] = rotation; }

bool TerrainDebug::heightmapChanged() {
  bool change = false;
  for (auto changed : _changedHeightmap) {
    if (changed) {
      change = true;
      break;
    }
  }
  return change;
}

void TerrainDebug::transfer(std::shared_ptr<CommandBuffer> commandBuffer) {
  _heightMap->copyFrom(_heightMapGPU, commandBuffer);
}

void TerrainDebug::draw(std::shared_ptr<CommandBuffer> commandBuffer) {
  int currentFrame = _engineState->getFrameInFlight();

  if (_changedHeightmap[currentFrame]) {
    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
    std::vector<VkDescriptorImageInfo> textureHeightmap(1);
    textureHeightmap[0].imageLayout = _heightMap->getImageView()->getImage()->getImageLayout();
    textureHeightmap[0].imageView = _heightMap->getImageView()->getImageView();
    textureHeightmap[0].sampler = _heightMap->getSampler()->getSampler();
    textureInfoColor[2] = textureHeightmap;
    _descriptorSetColor->updateImages(currentFrame, textureInfoColor);
    _changedHeightmap[currentFrame] = false;
  }

  auto drawTerrain = [&](std::shared_ptr<Pipeline> pipeline) {
    vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline->getPipeline());

    auto resolution = _engineState->getSettings()->getResolution();
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
      HeightLevelsDebug pushConstants;
      std::copy(std::begin(_heightLevels), std::end(_heightLevels), std::begin(pushConstants.heightLevels));
      pushConstants.patchEdge = _enableEdge;
      pushConstants.showLOD = _showLoD;
      pushConstants.cameraPosition = _gameState->getCameraManager()->getCurrentCamera()->getEye();
      pushConstants.tile = _pickedTile;
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(LoDConstants) + sizeof(PatchConstants),
                         sizeof(HeightLevelsDebug), &pushConstants);
    }

    // same buffer to both tessellation shaders because we're not going to change camera between these 2 stages
    BufferMVP cameraUBO{};
    cameraUBO.model = _model;
    cameraUBO.view = _gameState->getCameraManager()->getCurrentCamera()->getView();
    cameraUBO.projection = _gameState->getCameraManager()->getCurrentCamera()->getProjection();

    _cameraBuffer->getBuffer()[currentFrame]->setData(&cameraUBO);

    VkBuffer vertexBuffers[] = {_mesh[currentFrame]->getVertexBuffer()->getBuffer()->getData()};
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

    vkCmdDraw(commandBuffer->getCommandBuffer()[currentFrame], _mesh[currentFrame]->getVertexData().size(), 1, 0, 0);
  };

  if (_changeMesh[_engineState->getFrameInFlight()]) {
    _calculateMesh(_engineState->getFrameInFlight());
    _changeMesh[_engineState->getFrameInFlight()] = false;
  }

  if (_reallocatePatch[_engineState->getFrameInFlight()]) {
    _reallocatePatchDescription(_engineState->getFrameInFlight());
    _reallocatePatch[_engineState->getFrameInFlight()] = false;
  }

  if (_changePatch[_engineState->getFrameInFlight()]) {
    _updatePatchDescription(_engineState->getFrameInFlight());
    _changePatch[_engineState->getFrameInFlight()] = false;
  }

  auto pipeline = _pipeline;
  if (_drawType == DrawType::WIREFRAME) pipeline = _pipelineWireframe;
  if (_drawType == DrawType::NORMAL) pipeline = _pipelineNormalMesh;
  if (_drawType == DrawType::TANGENT) pipeline = _pipelineTangentMesh;
  drawTerrain(pipeline);
}

void TerrainDebug::drawDebug() {
  if (_gui->startTree("Terrain")) {
    _gui->drawText({"Tile: " + std::to_string(_pickedTile)});
    _gui->drawText({"Texture: " + std::to_string(_pickedPixel.x) + "x" + std::to_string(_pickedPixel.y)});
    std::map<std::string, int*> patchesNumber{{"Patch x", &_patchNumber.first}, {"Patch y", &_patchNumber.second}};
    if (_gui->drawInputInt(patchesNumber)) {
      // we can't change mesh here because we have to change it for all frames in flight eventually
      for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
        _changeMesh[i] = true;
        _reallocatePatch[i] = true;
      }
    }

    std::map<std::string, int*> angleList;
    angleList["##Angle"] = &_angleIndex;
    if (_gui->drawListBox({"0", "90", "180", "270"}, angleList, 4)) {
      auto rotation = glm::rotate(glm::mat4(1.f), glm::radians(90.f * _angleIndex), glm::vec3(0.f, 0.f, 1.f));
      if (_pickedTile > 0 && _pickedTile < _patchRotations.size()) {
        _patchRotations[_pickedTile] = rotation;
        for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
          _changePatch[i] = true;
        }
      }
    }

    std::map<std::string, int*> tesselationLevels{{"Tesselation min", &_minTessellationLevel},
                                                  {"Tesselation max", &_maxTessellationLevel}};
    if (_gui->drawInputInt(tesselationLevels)) {
      setTessellationLevel(_minTessellationLevel, _maxTessellationLevel);
    }

    if (_gui->drawCheckbox({{"Patches", &_showPatches}})) {
      patchEdge(_showPatches);
    }
    if (_gui->drawCheckbox({{"LoD", &_showLoD}})) {
      showLoD(_showLoD);
    }
    if (_gui->drawCheckbox({{"Wireframe", &_showWireframe}})) {
      setDrawType(DrawType::WIREFRAME);
      _showNormals = false;
    }
    if (_gui->drawCheckbox({{"Normal", &_showNormals}})) {
      setDrawType(DrawType::NORMAL);
      _showWireframe = false;
    }
    if (_showWireframe == false && _showNormals == false) {
      setDrawType(DrawType::FILL);
    }
    _gui->endTree();
  }
}

void TerrainDebug::cursorNotify(float xPos, float yPos) { _cursorPosition = glm::vec2{xPos, yPos}; }

void TerrainDebug::mouseNotify(int button, int action, int mods) {
#ifdef __ANDROID__
  if (button == 0 && action == 1) {
#else
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
#endif
    auto projection = _gameState->getCameraManager()->getCurrentCamera()->getProjection();
    // forward transformation
    // x, y, z, 1 * MVP -> clip?
    // clip / w -> NDC [-1, 1]
    //(NDC + 1) / 2 -> normalized screen [0, 1]
    // normalized screen * screen size -> screen

    // backward transformation
    // x, y
    glm::vec2 normalizedScreen = glm::vec2(
        (2.0f * _cursorPosition.x) / std::get<0>(_engineState->getSettings()->getResolution()) - 1.0f,
        1.0f - (2.0f * _cursorPosition.y) / std::get<1>(_engineState->getSettings()->getResolution()));
    glm::vec4 clipSpacePos = glm::vec4(normalizedScreen, -1.0f, 1.0f);  // Z = -1 to specify the near plane

    // Convert to camera (view) space
    glm::vec4 viewSpacePos = glm::inverse(_gameState->getCameraManager()->getCurrentCamera()->getProjection()) *
                             clipSpacePos;
    viewSpacePos = glm::vec4(viewSpacePos.x, viewSpacePos.y, -1.0f, 0.0f);

    // Convert to world space
    glm::vec4 worldSpaceRay = glm::inverse(_gameState->getCameraManager()->getCurrentCamera()->getView()) *
                              viewSpacePos;

    // normalize
    _rayDirection = glm::normalize(glm::vec3(worldSpaceRay));
    _rayOrigin = glm::vec3(glm::inverse(_gameState->getCameraManager()->getCurrentCamera()->getView())[3]);

    auto hit = _terrainPhysics->hit(_rayOrigin,
                                    _gameState->getCameraManager()->getCurrentCamera()->getFar() * _rayDirection);
    if (hit) {
      // find the corresponding patch number
      _pickedTile = _calculateTileByPosition(hit.value());
      _pickedPixel = _calculatePixelByPosition(hit.value());
    }
  }
}

void TerrainDebug::keyNotify(int key, int scancode, int action, int mods) {}

void TerrainDebug::charNotify(unsigned int code) {}

void TerrainDebug::scrollNotify(double xOffset, double yOffset) {
  _changeHeightmap(_pickedPixel, yOffset);
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) _changedHeightmap[i] = true;
}

Terrain::Terrain(std::shared_ptr<BufferImage> heightMap,
                 std::pair<int, int> patchNumber,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<GameState> gameState,
                 std::shared_ptr<EngineState> engineState) {
  setName("Terrain");
  _engineState = engineState;
  _gameState = gameState;
  _patchNumber = patchNumber;

  // needed for layout
  _defaultMaterialColor = std::make_shared<MaterialColor>(MaterialTarget::TERRAIN, commandBufferTransfer, engineState);
  _defaultMaterialColor->setBaseColor(std::vector{4, _gameState->getResourceManager()->getTextureOne()});
  _material = _defaultMaterialColor;
  _changedMaterial.resize(_engineState->getSettings()->getMaxFramesInFlight(), false);

  _heightMap = std::make_shared<Texture>(heightMap, _engineState->getSettings()->getLoadTextureAuxilaryFormat(),
                                         VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, VK_FILTER_LINEAR, commandBufferTransfer,
                                         engineState);
  auto [width, height] = _heightMap->getImageView()->getImage()->getResolution();
  std::vector<Vertex3D> vertices;
  _mesh = std::make_shared<MeshStatic3D>(engineState);
  for (int y = 0; y < patchNumber.second; y++) {
    for (int x = 0; x < patchNumber.first; x++) {
      // define patch: 4 points (square)
      Vertex3D vertex1{};
      vertex1.pos = glm::vec3(-width / 2.0f + (width - 1) * x / (float)patchNumber.first, 0.f,
                              -height / 2.0f + (height - 1) * y / (float)patchNumber.second);
      vertex1.texCoord = glm::vec2(x, y);
      vertices.push_back(vertex1);

      Vertex3D vertex2{};
      vertex2.pos = glm::vec3(-width / 2.0f + (width - 1) * (x + 1) / (float)patchNumber.first, 0.f,
                              -height / 2.0f + (height - 1) * y / (float)patchNumber.second);
      vertex2.texCoord = glm::vec2(x + 1, y);
      vertices.push_back(vertex2);

      Vertex3D vertex3{};
      vertex3.pos = glm::vec3(-width / 2.0f + (width - 1) * x / (float)patchNumber.first, 0.f,
                              -height / 2.0f + (height - 1) * (y + 1) / (float)patchNumber.second);
      vertex3.texCoord = glm::vec2(x, y + 1);
      vertices.push_back(vertex3);

      Vertex3D vertex4{};
      vertex4.pos = glm::vec3(-width / 2.0f + (width - 1) * (x + 1) / (float)patchNumber.first, 0.f,
                              -height / 2.0f + (height - 1) * (y + 1) / (float)patchNumber.second);
      vertex4.texCoord = glm::vec2(x + 1, y + 1);
      vertices.push_back(vertex4);
    }
  }
  _mesh->setVertices(vertices, commandBufferTransfer);

  std::map<std::string, VkPushConstantRange> defaultPushConstants;
  defaultPushConstants["control"] = LoDConstants::getPushConstant();
  defaultPushConstants["evaluate"] = PatchConstants::getPushConstant();
  defaultPushConstants["fragment"] = HeightLevels::getPushConstant();

  _renderPass = std::make_shared<RenderPass>(_engineState->getSettings(), _engineState->getDevice());
  _renderPass->initializeGraphic();
  _renderPassShadow = std::make_shared<RenderPass>(_engineState->getSettings(), _engineState->getDevice());
  _renderPassShadow->initializeLightDepth();

  _cameraBuffer = std::make_shared<UniformBuffer>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                  sizeof(BufferMVP), engineState);
  for (int i = 0; i < engineState->getSettings()->getMaxDirectionalLights(); i++) {
    _cameraBufferDepth.push_back({std::make_shared<UniformBuffer>(engineState->getSettings()->getMaxFramesInFlight(),
                                                                  sizeof(BufferMVP), engineState)});
  }

  for (int i = 0; i < engineState->getSettings()->getMaxPointLights(); i++) {
    std::vector<std::shared_ptr<UniformBuffer>> facesBuffer(6);
    for (int j = 0; j < 6; j++) {
      facesBuffer[j] = std::make_shared<UniformBuffer>(engineState->getSettings()->getMaxFramesInFlight(),
                                                       sizeof(BufferMVP), engineState);
    }
    _cameraBufferDepth.push_back(facesBuffer);
  }

  // layout for Shadows
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
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

    for (int d = 0; d < _engineState->getSettings()->getMaxDirectionalLights(); d++) {
      auto descriptorSetShadows = std::make_shared<DescriptorSet>(engineState->getSettings()->getMaxFramesInFlight(),
                                                                  descriptorSetLayout, engineState->getDescriptorPool(),
                                                                  engineState->getDevice());
      for (int i = 0; i < engineState->getSettings()->getMaxFramesInFlight(); i++) {
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

    for (int p = 0; p < _engineState->getSettings()->getMaxPointLights(); p++) {
      std::vector<std::shared_ptr<DescriptorSet>> facesSet;
      for (int f = 0; f < 6; f++) {
        auto descriptorSetShadows = std::make_shared<DescriptorSet>(
            engineState->getSettings()->getMaxFramesInFlight(), descriptorSetLayout, engineState->getDescriptorPool(),
            engineState->getDevice());
        for (int i = 0; i < engineState->getSettings()->getMaxFramesInFlight(); i++) {
          std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoNormalsMesh;
          std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
          std::vector<VkDescriptorBufferInfo> bufferInfoTesControl(1);
          bufferInfoTesControl[0].buffer =
              _cameraBufferDepth[p + _engineState->getSettings()->getMaxDirectionalLights()][f]
                  ->getBuffer()[i]
                  ->getData();
          bufferInfoTesControl[0].offset = 0;
          bufferInfoTesControl[0].range = sizeof(BufferMVP);
          bufferInfoNormalsMesh[0] = bufferInfoTesControl;

          std::vector<VkDescriptorBufferInfo> bufferInfoTesEval(1);
          bufferInfoTesEval[0].buffer =
              _cameraBufferDepth[p + _engineState->getSettings()->getMaxDirectionalLights()][f]
                  ->getBuffer()[i]
                  ->getData();
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
      auto shader = std::make_shared<Shader>(engineState);
      shader->add("shaders/terrain/terrainDepth_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/terrain/terrainDepth_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shader->add("shaders/terrain/terrainDepth_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

      std::map<std::string, VkPushConstantRange> defaultPushConstants;
      defaultPushConstants["control"] = LoDConstants::getPushConstant();
      defaultPushConstants["evaluate"] = PatchConstants::getPushConstant();

      _pipelineDirectional = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());
      _pipelineDirectional->createGraphicTerrainShadowGPU(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
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
      auto shader = std::make_shared<Shader>(engineState);
      shader->add("shaders/terrain/terrainDepth_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/terrain/terrainDepth_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shader->add("shaders/terrain/terrainDepth_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      shader->add("shaders/terrain/terrainDepth_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

      std::map<std::string, VkPushConstantRange> defaultPushConstants;
      defaultPushConstants["control"] = LoDConstants::getPushConstant();
      defaultPushConstants["evaluate"] = PatchConstants::getPushConstant();
      defaultPushConstants["fragment"] = DepthConstants::getPushConstant(sizeof(LoDConstants) + sizeof(PatchConstants));

      _pipelinePoint = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());
      // we use different culling here because camera looks upside down for lighting because of some specific cubemap Y
      // coordinate stuff
      _pipelinePoint->createGraphicTerrainShadowGPU(
          VK_CULL_MODE_FRONT_BIT, VK_POLYGON_MODE_FILL,
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

  // layout for Color
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
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
    _descriptorSetColor = std::make_shared<DescriptorSet>(engineState->getSettings()->getMaxFramesInFlight(),
                                                          descriptorSetLayout, engineState->getDescriptorPool(),
                                                          engineState->getDevice());
    setMaterial(_defaultMaterialColor);

    // initialize Color
    {
      auto shader = std::make_shared<Shader>(engineState);
      shader->add("shaders/terrain/terrainColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/terrain/terrainColor_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shader->add("shaders/terrain/terrainColor_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shader->add("shaders/terrain/terrainColor_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      _pipeline[MaterialType::COLOR] = std::make_shared<Pipeline>(_engineState->getSettings(),
                                                                  _engineState->getDevice());
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
    }
  }
  // layout for Phong
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
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
    _descriptorSetLayout[MaterialType::PHONG].push_back(
        {"globalPhong", _gameState->getLightManager()->getDSLGlobalTerrainPhong()});
    _descriptorSetPhong = std::make_shared<DescriptorSet>(engineState->getSettings()->getMaxFramesInFlight(),
                                                          descriptorSetLayout, engineState->getDescriptorPool(),
                                                          engineState->getDevice());
    // update descriptor set in setMaterial

    // initialize Phong
    {
      auto shader = std::make_shared<Shader>(engineState);
      shader->add("shaders/terrain/terrainColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/terrain/terrainPhong_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shader->add("shaders/terrain/terrainColor_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shader->add("shaders/terrain/terrainPhong_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      _pipeline[MaterialType::PHONG] = std::make_shared<Pipeline>(_engineState->getSettings(),
                                                                  _engineState->getDevice());
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
    }
  }
  // layout for PBR
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
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
    _descriptorSetLayout[MaterialType::PBR].push_back(
        {"globalPBR", _gameState->getLightManager()->getDSLGlobalTerrainPBR()});

    _descriptorSetPBR = std::make_shared<DescriptorSet>(engineState->getSettings()->getMaxFramesInFlight(),
                                                        descriptorSetLayout, engineState->getDescriptorPool(),
                                                        engineState->getDevice());
    // update descriptor set in setMaterial

    // initialize PBR
    {
      auto shader = std::make_shared<Shader>(engineState);
      shader->add("shaders/terrain/terrainColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/terrain/terrainPBR_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shader->add("shaders/terrain/terrainColor_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shader->add("shaders/terrain/terrainPhong_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      _pipeline[MaterialType::PBR] = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());
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
    }
  }
}

void Terrain::_updateColorDescriptor() {
  int currentFrame = _engineState->getFrameInFlight();
  auto material = std::dynamic_pointer_cast<MaterialColor>(_material);
  std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
  std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
  std::vector<VkDescriptorBufferInfo> bufferInfoCameraControl(1);
  bufferInfoCameraControl[0].buffer = _cameraBuffer->getBuffer()[currentFrame]->getData();
  bufferInfoCameraControl[0].offset = 0;
  bufferInfoCameraControl[0].range = sizeof(BufferMVP);
  bufferInfoColor[0] = bufferInfoCameraControl;

  std::vector<VkDescriptorBufferInfo> bufferInfoCameraEval(1);
  bufferInfoCameraEval[0].buffer = _cameraBuffer->getBuffer()[currentFrame]->getData();
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

  _descriptorSetColor->createCustom(currentFrame, bufferInfoColor, textureInfoColor);
}

void Terrain::_updatePhongDescriptor() {
  int currentFrame = _engineState->getFrameInFlight();
  auto material = std::dynamic_pointer_cast<MaterialPhong>(_material);
  std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
  std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
  std::vector<VkDescriptorBufferInfo> bufferInfoCameraControl(1);
  bufferInfoCameraControl[0].buffer = _cameraBuffer->getBuffer()[currentFrame]->getData();
  bufferInfoCameraControl[0].offset = 0;
  bufferInfoCameraControl[0].range = sizeof(BufferMVP);
  bufferInfoColor[0] = bufferInfoCameraControl;

  std::vector<VkDescriptorBufferInfo> bufferInfoCameraEval(1);
  bufferInfoCameraEval[0].buffer = _cameraBuffer->getBuffer()[currentFrame]->getData();
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
  bufferInfoCoefficients[0].buffer = material->getBufferCoefficients()->getBuffer()[currentFrame]->getData();
  bufferInfoCoefficients[0].offset = 0;
  bufferInfoCoefficients[0].range = material->getBufferCoefficients()->getBuffer()[currentFrame]->getSize();
  bufferInfoColor[6] = bufferInfoCoefficients;
  _descriptorSetPhong->createCustom(currentFrame, bufferInfoColor, textureInfoColor);
}

void Terrain::_updatePBRDescriptor() {
  int currentFrame = _engineState->getFrameInFlight();
  auto material = std::dynamic_pointer_cast<MaterialPBR>(_material);
  std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
  std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
  std::vector<VkDescriptorBufferInfo> bufferInfoCameraControl(1);
  bufferInfoCameraControl[0].buffer = _cameraBuffer->getBuffer()[currentFrame]->getData();
  bufferInfoCameraControl[0].offset = 0;
  bufferInfoCameraControl[0].range = sizeof(BufferMVP);
  bufferInfoColor[0] = bufferInfoCameraControl;

  std::vector<VkDescriptorBufferInfo> bufferInfoCameraEval(1);
  bufferInfoCameraEval[0].buffer = _cameraBuffer->getBuffer()[currentFrame]->getData();
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

  // TODO: this textures are part of global engineState for PBR
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
  bufferInfoCoefficients[0].buffer = material->getBufferCoefficients()->getBuffer()[currentFrame]->getData();
  bufferInfoCoefficients[0].offset = 0;
  bufferInfoCoefficients[0].range = material->getBufferCoefficients()->getBuffer()[currentFrame]->getSize();
  bufferInfoColor[12] = bufferInfoCoefficients;

  std::vector<VkDescriptorBufferInfo> bufferInfoAlphaCutoff(1);
  // write to binding = 0 for vertex shader
  bufferInfoAlphaCutoff[0].buffer = material->getBufferAlphaCutoff()->getBuffer()[currentFrame]->getData();
  bufferInfoAlphaCutoff[0].offset = 0;
  bufferInfoAlphaCutoff[0].range = material->getBufferAlphaCutoff()->getBuffer()[currentFrame]->getSize();
  bufferInfoColor[13] = bufferInfoAlphaCutoff;

  _descriptorSetPBR->createCustom(currentFrame, bufferInfoColor, textureInfoColor);
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
  if (_material) _material->unregisterUpdate(_descriptorSetPBR);
  material->registerUpdate(_descriptorSetPBR, {{MaterialTexture::COLOR, 3},
                                               {MaterialTexture::NORMAL, 4},
                                               {MaterialTexture::METALLIC, 5},
                                               {MaterialTexture::ROUGHNESS, 6},
                                               {MaterialTexture::OCCLUSION, 7},
                                               {MaterialTexture::EMISSIVE, 8},
                                               {MaterialTexture::IBL_DIFFUSE, 9},
                                               {MaterialTexture::IBL_SPECULAR, 10},
                                               {MaterialTexture::BRDF_SPECULAR, 11}});
  _material = material;
  _materialType = MaterialType::COLOR;
  for (int i = 0; i < _changedMaterial.size(); i++) {
    _changedMaterial[i] = true;
  }
}

void Terrain::setMaterial(std::shared_ptr<MaterialPhong> material) {
  if (_material) _material->unregisterUpdate(_descriptorSetPhong);
  material->registerUpdate(_descriptorSetPhong,
                           {{MaterialTexture::COLOR, 3}, {MaterialTexture::NORMAL, 4}, {MaterialTexture::SPECULAR, 5}});
  _material = material;
  _materialType = MaterialType::PHONG;
  for (int i = 0; i < _changedMaterial.size(); i++) {
    _changedMaterial[i] = true;
  }
}

void Terrain::setMaterial(std::shared_ptr<MaterialPBR> material) {
  if (_material) {
    _material->unregisterUpdate(_descriptorSetColor);
  }
  material->registerUpdate(_descriptorSetColor, {{MaterialTexture::COLOR, 3}});
  _material = material;
  _materialType = MaterialType::PBR;
  for (int i = 0; i < _changedMaterial.size(); i++) {
    _changedMaterial[i] = true;
  }
}

void Terrain::draw(std::shared_ptr<CommandBuffer> commandBuffer) {
  int currentFrame = _engineState->getFrameInFlight();
  if (_changedMaterial[currentFrame]) {
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
    _changedMaterial[currentFrame] = false;
  }

  auto drawTerrain = [&](std::shared_ptr<Pipeline> pipeline) {
    vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline->getPipeline());

    auto resolution = _engineState->getSettings()->getResolution();
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
      pushConstants.enableShadow = _enableShadow;
      pushConstants.enableLighting = _enableLighting;
      pushConstants.cameraPosition = _gameState->getCameraManager()->getCurrentCamera()->getEye();
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(LoDConstants) + sizeof(PatchConstants),
                         sizeof(HeightLevels), &pushConstants);
    }

    // same buffer to both tessellation shaders because we're not going to change camera between these 2 stages
    BufferMVP cameraUBO{};
    cameraUBO.model = _model;
    cameraUBO.view = _gameState->getCameraManager()->getCurrentCamera()->getView();
    cameraUBO.projection = _gameState->getCameraManager()->getCurrentCamera()->getProjection();

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
      vkCmdBindDescriptorSets(
          commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
          pipeline->getPipelineLayout(), 1, 1,
          &_gameState->getLightManager()->getDSGlobalTerrainPhong()->getDescriptorSets()[currentFrame], 0, nullptr);
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
      vkCmdBindDescriptorSets(
          commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
          pipeline->getPipelineLayout(), 1, 1,
          &_gameState->getLightManager()->getDSGlobalTerrainPBR()->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    vkCmdDraw(commandBuffer->getCommandBuffer()[currentFrame], _mesh->getVertexData().size(), 1, 0, 0);
  };

  auto pipeline = _pipeline[_materialType];
  drawTerrain(pipeline);
}

void Terrain::drawShadow(LightType lightType, int lightIndex, int face, std::shared_ptr<CommandBuffer> commandBuffer) {
  int currentFrame = _engineState->getFrameInFlight();
  auto pipeline = _pipelineDirectional;
  if (lightType == LightType::POINT) pipeline = _pipelinePoint;

  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->getPipeline());

  std::tuple<int, int> resolution;
  if (lightType == LightType::DIRECTIONAL) {
    resolution = _gameState->getLightManager()
                     ->getDirectionalLights()[lightIndex]
                     ->getDepthTexture()[currentFrame]
                     ->getImageView()
                     ->getImage()
                     ->getResolution();
  }
  if (lightType == LightType::POINT) {
    resolution = _gameState->getLightManager()
                     ->getPointLights()[lightIndex]
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
    pushConstants.lightPosition =
        _gameState->getLightManager()->getPointLights()[lightIndex]->getCamera()->getPosition();
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
    view = _gameState->getLightManager()->getDirectionalLights()[lightIndex]->getCamera()->getView();
    projection = _gameState->getLightManager()->getDirectionalLights()[lightIndex]->getCamera()->getProjection();
  }
  if (lightType == LightType::POINT) {
    lightIndexTotal += _engineState->getSettings()->getMaxDirectionalLights();
    view = _gameState->getLightManager()->getPointLights()[lightIndex]->getCamera()->getView(face);
    projection = _gameState->getLightManager()->getPointLights()[lightIndex]->getCamera()->getProjection();
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