#include "Primitive/TerrainComposition.h"
#undef far
#undef near
#include "stb_image_write.h"
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <nlohmann/json.hpp>
#include <fstream>

struct TesselationControlPush {
  int patchDimX;
  int patchDimY;
  int minTessellationLevel;
  int maxTessellationLevel;
  float minTesselationDistance;
  float maxTesselationDistance;
};

struct TesselationControlPushDepth {
  int minTessellationLevel;
  int maxTessellationLevel;
  float minTesselationDistance;
  float maxTesselationDistance;
};

struct TesselationEvaluationPush {
  int patchDimX;
  int patchDimY;
  float heightScale;
  float heightShift;
};

struct TesselationEvaluationPushDepth {
  float heightScale;
  float heightShift;
};

struct FragmentPush {
  int enableShadow;
  int enableLighting;
  glm::vec3 cameraPosition;
};

struct FragmentPushDebug {
  alignas(16) int patchEdge;
  int showLOD;
  int enableShadow;
  int enableLighting;
  int tile;
};

TerrainCompositionDebug::TerrainCompositionDebug(std::shared_ptr<ImageCPU<uint8_t>> heightMapCPU,
                                                 std::pair<int, int> patchNumber,
                                                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                                                 std::shared_ptr<GUI> gui,
                                                 std::shared_ptr<GameState> gameState,
                                                 std::shared_ptr<EngineState> engineState) {
  setName("TerrainComposition");
  _engineState = engineState;
  _gameState = gameState;
  _patchNumber = patchNumber;
  _gui = gui;
  _changedMaterial.resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
  // needed for layout
  _defaultMaterialColor = std::make_shared<MaterialColor>(MaterialTarget::TERRAIN, commandBufferTransfer, engineState);
  _defaultMaterialColor->setBaseColor(std::vector{4, _gameState->getResourceManager()->getTextureOne()});
  _material = _defaultMaterialColor;
  _heightMapCPU = heightMapCPU;
  _heightMapGPU = _gameState->getResourceManager()->loadImageGPU<uint8_t>({heightMapCPU});
  _changedHeightmap.resize(_engineState->getSettings()->getMaxFramesInFlight());
  _heightMap = std::make_shared<Texture>(_heightMapGPU, _engineState->getSettings()->getLoadTextureAuxilaryFormat(),
                                         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, VK_FILTER_LINEAR,
                                         commandBufferTransfer, engineState);

  _changeMesh.resize(engineState->getSettings()->getMaxFramesInFlight());
  _mesh.resize(engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _mesh[i] = std::make_shared<MeshDynamic3D>(engineState);
    _calculateMesh(i);
  }

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
      shaderNormal->add("shaders/terrain/terrainDepth_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shaderNormal->add("shaders/terrain/terrainNormal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shaderNormal->add("shaders/terrain/terrainDepth_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shaderNormal->add("shaders/terrain/terrainNormal_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      shaderNormal->add("shaders/terrain/terrainNormal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

      _pipelineNormalMesh = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());

      std::map<std::string, VkPushConstantRange> pushConstants;
      pushConstants["controlDepth"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
          .offset = 0,
          .size = sizeof(TesselationControlPushDepth),
      };
      pushConstants["evaluate"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
          .offset = sizeof(TesselationControlPushDepth),
          .size = sizeof(TesselationEvaluationPush),
      };
      _pipelineNormalMesh->createGraphicTerrain(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
          {shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
          _descriptorSetLayoutNormalsMesh, pushConstants, _mesh[0]->getBindingDescription(),
          _mesh[0]->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                    {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPass);
    }

    // initialize Tangent (per vertex)
    {
      auto shaderNormal = std::make_shared<Shader>(engineState);
      shaderNormal->add("shaders/terrain/terrainDepth_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shaderNormal->add("shaders/terrain/terrainNormal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shaderNormal->add("shaders/terrain/terrainDepth_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shaderNormal->add("shaders/terrain/terrainTangent_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      shaderNormal->add("shaders/terrain/terrainNormal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

      _pipelineTangentMesh = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());

      std::map<std::string, VkPushConstantRange> pushConstants;
      pushConstants["controlDepth"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
          .offset = 0,
          .size = sizeof(TesselationControlPushDepth),
      };
      pushConstants["evaluate"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
          .offset = sizeof(TesselationControlPushDepth),
          .size = sizeof(TesselationEvaluationPush),
      };
      _pipelineTangentMesh->createGraphicTerrain(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
          {shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
           shaderNormal->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
          _descriptorSetLayoutNormalsMesh, pushConstants, _mesh[0]->getBindingDescription(),
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
    layoutColor[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutColor[1].pImmutableSamplers = nullptr;
    layoutColor[1].stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    layoutColor[2].binding = 2;
    layoutColor[2].descriptorCount = 1;
    layoutColor[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutColor[2].pImmutableSamplers = nullptr;
    layoutColor[2].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    layoutColor[3].binding = 3;
    layoutColor[3].descriptorCount = 1;
    layoutColor[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutColor[3].pImmutableSamplers = nullptr;
    layoutColor[3].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    layoutColor[4].binding = 4;
    layoutColor[4].descriptorCount = 4;
    layoutColor[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutColor[4].pImmutableSamplers = nullptr;
    layoutColor[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayout->createCustom(layoutColor);
    _descriptorSetLayout.push_back({"color", descriptorSetLayout});
    _descriptorSetColor = std::make_shared<DescriptorSet>(engineState->getSettings()->getMaxFramesInFlight(),
                                                          descriptorSetLayout, engineState->getDescriptorPool(),
                                                          engineState->getDevice());
    setMaterial(_defaultMaterialColor);

    // initialize Color
    {
      auto shader = std::make_shared<Shader>(engineState);
      shader->add("shaders/terrain/composition/terrainDebug_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/terrain/composition/terrainDebug_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shader->add("shaders/terrain/composition/terrainDebug_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shader->add("shaders/terrain/composition/terrainDebug_evaluation.spv",
                  VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      _pipeline = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());

      std::map<std::string, VkPushConstantRange> pushConstants;
      pushConstants["control"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
          .offset = 0,
          .size = sizeof(TesselationControlPush),
      };
      pushConstants["evaluateDepth"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
          .offset = sizeof(TesselationControlPush),
          .size = sizeof(TesselationEvaluationPushDepth),
      };
      pushConstants["fragment"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = sizeof(TesselationControlPush) + sizeof(TesselationEvaluationPushDepth),
          .size = sizeof(FragmentPush),
      };
      _pipeline->createGraphicTerrain(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
          _descriptorSetLayout, pushConstants, _mesh[0]->getBindingDescription(),
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
          _descriptorSetLayout, pushConstants, _mesh[0]->getBindingDescription(),
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

int TerrainCompositionDebug::_saveAuxilary(std::string path) {
  nlohmann::json outputJSON;
  outputJSON["rotation"] = _patchRotationsIndex;
  outputJSON["textures"] = _patchTextures;
  outputJSON["patches"] = _patchNumber;
  std::ofstream file(path + ".json");
  file << outputJSON;
  return true;
}

void TerrainCompositionDebug::_loadAuxilary(std::string path) {
  std::ifstream file(path + ".json");
  nlohmann::json inputJSON;
  file >> inputJSON;
  bool changePatches = false;
  if (inputJSON["rotation"].size() != _patchRotationsIndex.size()) {
    changePatches = true;
    _patchRotationsIndex.resize(inputJSON["rotation"].size());
  }
  for (int i = 0; i < inputJSON["rotation"].size(); i++) {
    _patchRotationsIndex[i] = inputJSON["rotation"][i];
  }
  if (inputJSON["textures"].size() != _patchTextures.size()) {
    changePatches = true;
    _patchTextures.resize(inputJSON["textures"].size());
  }
  for (int i = 0; i < inputJSON["textures"].size(); i++) {
    _patchTextures[i] = inputJSON["textures"][i];
  }
  std::pair<int, int> newPatches = inputJSON["patches"];
  if (newPatches.first != _patchNumber.first || newPatches.second != _patchNumber.second) {
    changePatches = true;
    _patchNumber = newPatches;
  }

  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _changePatch[i] = true;
  }

  if (changePatches) {
    for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
      _changeMesh[i] = true;
      // needed for rotation. Number of patches has been changed, need to reallocate and set.
      _reallocatePatch[i] = true;
      _changedMaterial[i] = true;
    }
  }
}

void TerrainCompositionDebug::_updateColorDescriptor() {
  int currentFrame = _engineState->getFrameInFlight();
  auto material = std::dynamic_pointer_cast<MaterialColor>(_material);
  std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
  std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
  std::vector<VkDescriptorBufferInfo> bufferInfoCameraControl(1);
  bufferInfoCameraControl[0].buffer = _cameraBuffer->getBuffer()[currentFrame]->getData();
  bufferInfoCameraControl[0].offset = 0;
  bufferInfoCameraControl[0].range = sizeof(BufferMVP);
  bufferInfoColor[0] = bufferInfoCameraControl;

  std::vector<VkDescriptorBufferInfo> bufferPatchInfo(1);
  bufferPatchInfo[0].buffer = _patchDescriptionSSBO[currentFrame]->getData();
  bufferPatchInfo[0].offset = 0;
  bufferPatchInfo[0].range = _patchDescriptionSSBO[currentFrame]->getSize();
  bufferInfoColor[1] = bufferPatchInfo;

  std::vector<VkDescriptorBufferInfo> bufferInfoCameraEval(1);
  bufferInfoCameraEval[0].buffer = _cameraBuffer->getBuffer()[currentFrame]->getData();
  bufferInfoCameraEval[0].offset = 0;
  bufferInfoCameraEval[0].range = sizeof(BufferMVP);
  bufferInfoColor[2] = bufferInfoCameraEval;

  std::vector<VkDescriptorImageInfo> textureHeightmap(1);
  textureHeightmap[0].imageLayout = _heightMap->getImageView()->getImage()->getImageLayout();
  textureHeightmap[0].imageView = _heightMap->getImageView()->getImageView();
  textureHeightmap[0].sampler = _heightMap->getSampler()->getSampler();
  textureInfoColor[3] = textureHeightmap;

  std::vector<VkDescriptorImageInfo> textureBaseColor(material->getBaseColor().size());
  for (int j = 0; j < material->getBaseColor().size(); j++) {
    textureBaseColor[j].imageLayout = material->getBaseColor()[j]->getImageView()->getImage()->getImageLayout();
    textureBaseColor[j].imageView = material->getBaseColor()[j]->getImageView()->getImageView();
    textureBaseColor[j].sampler = material->getBaseColor()[j]->getSampler()->getSampler();
  }
  textureInfoColor[4] = textureBaseColor;

  _descriptorSetColor->createCustom(currentFrame, bufferInfoColor, textureInfoColor);
}

void TerrainCompositionDebug::_reallocatePatchDescription(int currentFrame) {
  _patchTextures = std::vector<int>(_patchNumber.first * _patchNumber.second, 0);
  auto [width, height] = _heightMap->getImageView()->getImage()->getResolution();
  //
  for (int y = 0; y < _patchNumber.second; y++)
    for (int x = 0; x < _patchNumber.first; x++) {
      int patchX0 = (float)x / _patchNumber.first * width;
      int patchY0 = (float)y / _patchNumber.second * height;
      int patchX1 = (float)(x + 1) / _patchNumber.first * width;
      int patchY1 = (float)(y + 1) / _patchNumber.second * height;

      int heightLevel = 0;
      for (int i = patchY0; i < patchY1; i++) {
        for (int j = patchX0; j < patchX1; j++) {
          int textureCoord = (j + i * width) * _heightMapCPU->getChannels();
          heightLevel = std::max(heightLevel, (int)_heightMapCPU->getData()[textureCoord]);
        }
      }

      int index = x + y * _patchNumber.first;
      if (heightLevel < _heightLevels[0]) {
        _patchTextures[index] = 0;
      } else if (heightLevel < _heightLevels[1]) {
        _patchTextures[index] = 1;
      } else if (heightLevel < _heightLevels[2]) {
        _patchTextures[index] = 2;
      } else {
        _patchTextures[index] = 3;
      }
    }

  _patchRotationsIndex = std::vector<int>(_patchNumber.first * _patchNumber.second, 0);
  _patchDescriptionSSBO[currentFrame] = std::make_shared<Buffer>(
      _patchNumber.first * _patchNumber.second * sizeof(PatchDescription), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);
}

void TerrainCompositionDebug::_updatePatchDescription(int currentFrame) {
  std::vector<PatchDescription> patchData(_patchNumber.first * _patchNumber.second);
  for (int i = 0; i < patchData.size(); i++) {
    patchData[i] = PatchDescription({.rotation = 90 * _patchRotationsIndex[i], .textureID = _patchTextures[i]});
  }
  // fill only number of lights because it's stub
  _patchDescriptionSSBO[currentFrame]->setData(patchData.data(), patchData.size() * sizeof(PatchDescription));
}

void TerrainCompositionDebug::draw(std::shared_ptr<CommandBuffer> commandBuffer) {
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
    if (pipeline->getPushConstants().find("control") != pipeline->getPushConstants().end()) {
      TesselationControlPush pushConstants;
      pushConstants.patchDimX = _patchNumber.first;
      pushConstants.patchDimY = _patchNumber.second;
      pushConstants.minTesselationDistance = _minTesselationDistance;
      pushConstants.maxTesselationDistance = _maxTesselationDistance;
      pushConstants.minTessellationLevel = _minTessellationLevel;
      pushConstants.maxTessellationLevel = _maxTessellationLevel;
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0, sizeof(TesselationControlPush), &pushConstants);
    }

    if (pipeline->getPushConstants().find("controlDepth") != pipeline->getPushConstants().end()) {
      TesselationControlPushDepth pushConstants;
      pushConstants.minTesselationDistance = _minTesselationDistance;
      pushConstants.maxTesselationDistance = _maxTesselationDistance;
      pushConstants.minTessellationLevel = _minTessellationLevel;
      pushConstants.maxTessellationLevel = _maxTessellationLevel;
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0, sizeof(TesselationControlPushDepth),
                         &pushConstants);
    }

    if (pipeline->getPushConstants().find("evaluate") != pipeline->getPushConstants().end()) {
      TesselationEvaluationPush pushConstants;
      pushConstants.patchDimX = _patchNumber.first;
      pushConstants.patchDimY = _patchNumber.second;
      pushConstants.heightScale = _heightScale;
      pushConstants.heightShift = _heightShift;
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, sizeof(TesselationControlPushDepth),
                         sizeof(TesselationEvaluationPush), &pushConstants);
    }

    if (pipeline->getPushConstants().find("evaluateDepth") != pipeline->getPushConstants().end()) {
      TesselationEvaluationPushDepth pushConstants;
      pushConstants.heightScale = _heightScale;
      pushConstants.heightShift = _heightShift;
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, sizeof(TesselationControlPush),
                         sizeof(TesselationEvaluationPushDepth), &pushConstants);
    }

    if (pipeline->getPushConstants().find("fragment") != pipeline->getPushConstants().end()) {
      FragmentPushDebug pushConstants;
      pushConstants.patchEdge = _enableEdge;
      pushConstants.showLOD = _showLoD;
      pushConstants.enableShadow = _enableShadow;
      pushConstants.enableLighting = _enableLighting;
      pushConstants.tile = _pickedTile;
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_FRAGMENT_BIT,
                         sizeof(TesselationControlPush) + sizeof(TesselationEvaluationPushDepth),
                         sizeof(FragmentPushDebug), &pushConstants);
    }

    // same buffer to both tessellation shaders because we're not going to change camera between these 2 stages
    BufferMVP cameraUBO{};
    cameraUBO.model = getModel();
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

  if (_changedMaterial[currentFrame]) {
    _updateColorDescriptor();
    _changedMaterial[currentFrame] = false;
  }

  if (_changedHeightmap[currentFrame]) {
    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
    std::vector<VkDescriptorImageInfo> textureHeightmap(1);
    textureHeightmap[0].imageLayout = _heightMap->getImageView()->getImage()->getImageLayout();
    textureHeightmap[0].imageView = _heightMap->getImageView()->getImageView();
    textureHeightmap[0].sampler = _heightMap->getSampler()->getSampler();
    textureInfoColor[3] = textureHeightmap;
    _descriptorSetColor->updateImages(currentFrame, textureInfoColor);
    _changedHeightmap[currentFrame] = false;
  }

  auto pipeline = _pipeline;
  if (_drawType == DrawType::WIREFRAME) pipeline = _pipelineWireframe;
  if (_drawType == DrawType::NORMAL) pipeline = _pipelineNormalMesh;
  if (_drawType == DrawType::TANGENT) pipeline = _pipelineTangentMesh;
  drawTerrain(pipeline);
}

void TerrainCompositionDebug::drawDebug() {
  if (_gui->startTree("Terrain")) {
    _gui->drawText({"Tile: " + std::to_string(_pickedTile)});
    _gui->drawText({"Texture: " + std::to_string(_pickedPixel.x) + "x" + std::to_string(_pickedPixel.y)});
    _gui->drawText({"Click: " + std::to_string(_clickPosition.x) + "x" + std::to_string(_clickPosition.y)});
    glm::vec3 physicsHit = glm::vec3(-1.f);
    if (_hitCoords) physicsHit = _hitCoords.value();
    _gui->drawText({"Hit: " + std::to_string(physicsHit.x) + "x" + std::to_string(physicsHit.y) + "x" +
                    std::to_string(physicsHit.z)});
    std::map<std::string, int*> patchesNumber{{"Patch x", &_patchNumber.first}, {"Patch y", &_patchNumber.second}};
    if (_gui->drawInputInt(patchesNumber)) {
      // we can't change mesh here because we have to change it for all frames in flight eventually
      for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
        _changeMesh[i] = true;
        // needed for rotation. Number of patches has been changed, need to reallocate and set.
        _reallocatePatch[i] = true;
        _changePatch[i] = true;
        _changedMaterial[i] = true;
      }
    }
    std::map<std::string, int*> tesselationLevels{{"Tesselation min", &_minTessellationLevel},
                                                  {"Tesselation max", &_maxTessellationLevel}};
    if (_gui->drawInputInt(tesselationLevels)) {
      setTessellationLevel(_minTessellationLevel, _maxTessellationLevel);
    }

    std::map<std::string, int*> angleList;
    angleList["##Angle"] = &_angleIndex;
    if (_gui->drawListBox({"0", "90", "180", "270"}, angleList, 4)) {
      if (_pickedTile > 0 && _pickedTile < _patchRotationsIndex.size()) {
        _patchRotationsIndex[_pickedTile] = _angleIndex;
        for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
          _changePatch[i] = true;
        }
      }
    }

    std::map<std::string, int*> textureList;
    textureList["##Texture"] = &_textureIndex;
    if (_gui->drawListBox({"0", "1", "2", "3"}, textureList, 4)) {
      if (_pickedTile > 0 && _pickedTile < _patchTextures.size()) {
        _patchTextures[_pickedTile] = _textureIndex;
        for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
          _changePatch[i] = true;
        }
      }
    }

    _gui->drawInputText("##Path", _terrainPath, sizeof(_terrainPath));

    if (_gui->drawButton("Save terrain")) {
      _saveHeightmap(std::string(_terrainPath));
      _saveAuxilary(std::string(_terrainPath));
    }

    if (_gui->drawButton("Load terrain")) {
      _loadHeightmap(std::string(_terrainPath));
      _loadAuxilary(std::string(_terrainPath));
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

void TerrainCompositionDebug::cursorNotify(float xPos, float yPos) { _cursorPosition = glm::vec2{xPos, yPos}; }

void TerrainCompositionDebug::mouseNotify(int button, int action, int mods) {
#ifdef __ANDROID__
  if (button == 0 && action == 1) {
#else
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
#endif
    _clickPosition = _cursorPosition;
    _hitCoords = _terrainPhysics->getHit(_cursorPosition);
    if (_hitCoords) {
      // find the corresponding patch number
      _pickedTile = _calculateTileByPosition(_hitCoords.value());
      _pickedPixel = _calculatePixelByPosition(_hitCoords.value());
      // reset selected item in angles list
      _angleIndex = _patchRotationsIndex[_pickedTile];
      _textureIndex = _patchTextures[_pickedTile];
    }
  }
}

void TerrainCompositionDebug::keyNotify(int key, int scancode, int action, int mods) {}

void TerrainCompositionDebug::charNotify(unsigned int code) {}

void TerrainCompositionDebug::scrollNotify(double xOffset, double yOffset) {
  _changeHeightmap(_pickedPixel, yOffset);
  // need to recreate TerrainPhysics because heightmapCPU was updated
  _terrainPhysics->reset(_heightMapCPU);
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) _changedHeightmap[i] = true;
}

TerrainComposition::TerrainComposition(std::shared_ptr<ImageCPU<uint8_t>> heightMapCPU,
                                       std::shared_ptr<GameState> gameState,
                                       std::shared_ptr<EngineState> engineState) {
  setName("Terrain");
  _engineState = engineState;
  _gameState = gameState;
  _heightMapCPU = heightMapCPU;
}

void TerrainComposition::initialize(std::shared_ptr<CommandBuffer> commandBuffer) {
  // needed for layout
  _defaultMaterialColor = std::make_shared<MaterialColor>(MaterialTarget::TERRAIN, commandBuffer, _engineState);
  _defaultMaterialColor->setBaseColor(std::vector{4, _gameState->getResourceManager()->getTextureOne()});
  _material = _defaultMaterialColor;
  _changedMaterial.resize(_engineState->getSettings()->getMaxFramesInFlight(), false);

  _heightMapGPU = _gameState->getResourceManager()->loadImageGPU<uint8_t>({_heightMapCPU});

  _heightMap = std::make_shared<Texture>(_heightMapGPU, _engineState->getSettings()->getLoadTextureAuxilaryFormat(),
                                         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, VK_FILTER_LINEAR, commandBuffer,
                                         _engineState);
  auto [width, height] = _heightMap->getImageView()->getImage()->getResolution();
  _mesh = std::make_shared<MeshStatic3D>(_engineState);
  _calculateMesh(commandBuffer);

  _renderPass = std::make_shared<RenderPass>(_engineState->getSettings(), _engineState->getDevice());
  _renderPass->initializeGraphic();
  _renderPassShadow = std::make_shared<RenderPass>(_engineState->getSettings(), _engineState->getDevice());
  _renderPassShadow->initializeLightDepth();

  _cameraBuffer = std::make_shared<UniformBuffer>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                  sizeof(BufferMVP), _engineState);
  for (int i = 0; i < _engineState->getSettings()->getMaxDirectionalLights(); i++) {
    _cameraBufferDepth.push_back({std::make_shared<UniformBuffer>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                                  sizeof(BufferMVP), _engineState)});
  }

  for (int i = 0; i < _engineState->getSettings()->getMaxPointLights(); i++) {
    std::vector<std::shared_ptr<UniformBuffer>> facesBuffer(6);
    for (int j = 0; j < 6; j++) {
      facesBuffer[j] = std::make_shared<UniformBuffer>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                       sizeof(BufferMVP), _engineState);
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
      auto descriptorSetShadows = std::make_shared<DescriptorSet>(
          _engineState->getSettings()->getMaxFramesInFlight(), descriptorSetLayout, _engineState->getDescriptorPool(),
          _engineState->getDevice());
      for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
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
            _engineState->getSettings()->getMaxFramesInFlight(), descriptorSetLayout, _engineState->getDescriptorPool(),
            _engineState->getDevice());
        for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
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
      auto shader = std::make_shared<Shader>(_engineState);
      shader->add("shaders/terrain/terrainDepth_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/terrain/terrainDepth_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shader->add("shaders/terrain/terrainDepth_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      shader->add("shaders/terrain/terrainDepthDirectional_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

      std::map<std::string, VkPushConstantRange> pushConstants;
      pushConstants["controlDepth"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
          .offset = 0,
          .size = sizeof(TesselationControlPushDepth),
      };
      pushConstants["evaluateDepth"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
          .offset = sizeof(TesselationControlPushDepth),
          .size = sizeof(TesselationEvaluationPushDepth),
      };

      _pipelineDirectional = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());
      _pipelineDirectional->createGraphicTerrainShadowGPU(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          _descriptorSetLayoutShadows, pushConstants, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPassShadow);
    }

    // initialize Shadows (Point)
    {
      auto shader = std::make_shared<Shader>(_engineState);
      shader->add("shaders/terrain/terrainDepth_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/terrain/terrainDepth_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shader->add("shaders/terrain/terrainDepth_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      shader->add("shaders/terrain/terrainDepthPoint_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

      std::map<std::string, VkPushConstantRange> pushConstants;
      pushConstants["controlDepth"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
          .offset = 0,
          .size = sizeof(TesselationControlPushDepth),
      };
      pushConstants["evaluateDepth"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
          .offset = sizeof(TesselationControlPushDepth),
          .size = sizeof(TesselationEvaluationPushDepth),
      };
      pushConstants["fragmentDepth"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = sizeof(TesselationControlPushDepth) +
                    std::max(sizeof(TesselationEvaluationPushDepth), std::alignment_of<DepthConstants>::value),
          .size = sizeof(DepthConstants),
      };

      _pipelinePoint = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());
      // we use different culling here because camera looks upside down for lighting because of some specific cubemap Y
      // coordinate stuff
      _pipelinePoint->createGraphicTerrainShadowGPU(
          VK_CULL_MODE_FRONT_BIT, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          _descriptorSetLayoutShadows, pushConstants, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPassShadow);
    }
  }

  if (_patchRotationsIndex.size() == 0) _fillPatchRotationsDescription();
  if (_patchTextures.size() == 0) _fillPatchTexturesDescription();

  _patchDescriptionSSBO.resize(_engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _allocatePatchDescription(i);
    _updatePatchDescription(i);
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
    layoutColor[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutColor[1].pImmutableSamplers = nullptr;
    layoutColor[1].stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    layoutColor[2].binding = 2;
    layoutColor[2].descriptorCount = 1;
    layoutColor[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutColor[2].pImmutableSamplers = nullptr;
    layoutColor[2].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    layoutColor[3].binding = 3;
    layoutColor[3].descriptorCount = 1;
    layoutColor[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutColor[3].pImmutableSamplers = nullptr;
    layoutColor[3].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    layoutColor[4].binding = 4;
    layoutColor[4].descriptorCount = 4;
    layoutColor[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutColor[4].pImmutableSamplers = nullptr;
    layoutColor[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    descriptorSetLayout->createCustom(layoutColor);
    _descriptorSetLayout[MaterialType::COLOR].push_back({"color", descriptorSetLayout});
    _descriptorSetColor = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                          descriptorSetLayout, _engineState->getDescriptorPool(),
                                                          _engineState->getDevice());
    setMaterial(_defaultMaterialColor);

    // initialize Color
    {
      auto shader = std::make_shared<Shader>(_engineState);
      shader->add("shaders/terrain/composition/terrainColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/terrain/composition/terrainColor_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shader->add("shaders/terrain/composition/terrainColor_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shader->add("shaders/terrain/composition/terrainColor_evaluation.spv",
                  VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      _pipeline[MaterialType::COLOR] = std::make_shared<Pipeline>(_engineState->getSettings(),
                                                                  _engineState->getDevice());

      std::map<std::string, VkPushConstantRange> pushConstants;
      pushConstants["control"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
          .offset = 0,
          .size = sizeof(TesselationControlPush),
      };
      pushConstants["evaluateDepth"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
          .offset = sizeof(TesselationControlPush),
          .size = sizeof(TesselationEvaluationPushDepth),
      };
      pushConstants["fragmentColor"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = sizeof(TesselationControlPush) + sizeof(TesselationEvaluationPushDepth),
          .size = sizeof(FragmentPush),
      };

      _pipeline[MaterialType::COLOR]->createGraphicTerrain(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
          _descriptorSetLayout[MaterialType::COLOR], pushConstants, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPass);
    }
  }
  // layout for Phong
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutPhong(8);
    layoutPhong[0].binding = 0;
    layoutPhong[0].descriptorCount = 1;
    layoutPhong[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPhong[0].pImmutableSamplers = nullptr;
    layoutPhong[0].stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    layoutPhong[1].binding = 1;
    layoutPhong[1].descriptorCount = 1;
    layoutPhong[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutPhong[1].pImmutableSamplers = nullptr;
    layoutPhong[1].stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    layoutPhong[2].binding = 2;
    layoutPhong[2].descriptorCount = 1;
    layoutPhong[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPhong[2].pImmutableSamplers = nullptr;
    layoutPhong[2].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    layoutPhong[3].binding = 3;
    layoutPhong[3].descriptorCount = 1;
    layoutPhong[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPhong[3].pImmutableSamplers = nullptr;
    layoutPhong[3].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
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
    layoutPhong[6].descriptorCount = 4;
    layoutPhong[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPhong[6].pImmutableSamplers = nullptr;
    layoutPhong[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPhong[7].binding = 7;
    layoutPhong[7].descriptorCount = 1;
    layoutPhong[7].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPhong[7].pImmutableSamplers = nullptr;
    layoutPhong[7].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayout->createCustom(layoutPhong);

    _descriptorSetLayout[MaterialType::PHONG].push_back({"phong", descriptorSetLayout});
    _descriptorSetLayout[MaterialType::PHONG].push_back(
        {"globalPhong", _gameState->getLightManager()->getDSLGlobalTerrainPhong()});
    _descriptorSetPhong = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                          descriptorSetLayout, _engineState->getDescriptorPool(),
                                                          _engineState->getDevice());
    // update descriptor set in setMaterial

    // initialize Phong
    {
      auto shader = std::make_shared<Shader>(_engineState);
      shader->add("shaders/terrain/composition/terrainColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/terrain/composition/terrainPhong_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shader->add("shaders/terrain/composition/terrainColor_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shader->add("shaders/terrain/composition/terrainPhong_evaluation.spv",
                  VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      _pipeline[MaterialType::PHONG] = std::make_shared<Pipeline>(_engineState->getSettings(),
                                                                  _engineState->getDevice());

      std::map<std::string, VkPushConstantRange> pushConstants;
      pushConstants["control"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
          .offset = 0,
          .size = sizeof(TesselationControlPush),
      };
      pushConstants["evaluate"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
          .offset = sizeof(TesselationControlPush),
          .size = sizeof(TesselationEvaluationPush),
      };
      pushConstants["fragment"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = sizeof(TesselationControlPush) + sizeof(TesselationEvaluationPush),
          .size = sizeof(FragmentPush),
      };

      _pipeline[MaterialType::PHONG]->createGraphicTerrain(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
          _descriptorSetLayout[MaterialType::PHONG], pushConstants, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPass);
    }
  }
  // layout for PBR
  {
    auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutPBR(15);
    layoutPBR[0].binding = 0;
    layoutPBR[0].descriptorCount = 1;
    layoutPBR[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPBR[0].pImmutableSamplers = nullptr;
    layoutPBR[0].stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    layoutPBR[1].binding = 1;
    layoutPBR[1].descriptorCount = 1;
    layoutPBR[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutPBR[1].pImmutableSamplers = nullptr;
    layoutPBR[1].stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    layoutPBR[2].binding = 2;
    layoutPBR[2].descriptorCount = 1;
    layoutPBR[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPBR[2].pImmutableSamplers = nullptr;
    layoutPBR[2].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    layoutPBR[3].binding = 3;
    layoutPBR[3].descriptorCount = 1;
    layoutPBR[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[3].pImmutableSamplers = nullptr;
    layoutPBR[3].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
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
    layoutPBR[9].descriptorCount = 4;
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
    layoutPBR[12].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[12].pImmutableSamplers = nullptr;
    layoutPBR[12].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[13].binding = 13;
    layoutPBR[13].descriptorCount = 1;
    layoutPBR[13].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPBR[13].pImmutableSamplers = nullptr;
    layoutPBR[13].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[14].binding = 14;
    layoutPBR[14].descriptorCount = 1;
    layoutPBR[14].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutPBR[14].pImmutableSamplers = nullptr;
    layoutPBR[14].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayout->createCustom(layoutPBR);
    _descriptorSetLayout[MaterialType::PBR].push_back({"pbr", descriptorSetLayout});
    _descriptorSetLayout[MaterialType::PBR].push_back(
        {"globalPBR", _gameState->getLightManager()->getDSLGlobalTerrainPBR()});

    _descriptorSetPBR = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                        descriptorSetLayout, _engineState->getDescriptorPool(),
                                                        _engineState->getDevice());
    // update descriptor set in setMaterial

    // initialize PBR
    {
      auto shader = std::make_shared<Shader>(_engineState);
      shader->add("shaders/terrain/composition/terrainColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/terrain/composition/terrainPBR_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shader->add("shaders/terrain/composition/terrainColor_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
      shader->add("shaders/terrain/composition/terrainPhong_evaluation.spv",
                  VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
      _pipeline[MaterialType::PBR] = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());

      std::map<std::string, VkPushConstantRange> pushConstants;
      pushConstants["control"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
          .offset = 0,
          .size = sizeof(TesselationControlPush),
      };
      pushConstants["evaluate"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
          .offset = sizeof(TesselationControlPush),
          .size = sizeof(TesselationEvaluationPush),
      };
      pushConstants["fragment"] = VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = sizeof(TesselationControlPush) + sizeof(TesselationEvaluationPush),
          .size = sizeof(FragmentPush),
      };

      _pipeline[MaterialType::PBR]->createGraphicTerrain(
          VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
          _descriptorSetLayout[MaterialType::PBR], pushConstants, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)}}),
          _renderPass);
    }
  }
}

void TerrainComposition::_fillPatchTexturesDescription() {
  _patchTextures = std::vector<int>(_patchNumber.first * _patchNumber.second, 0);
  auto [width, height] = _heightMap->getImageView()->getImage()->getResolution();
  //
  for (int y = 0; y < _patchNumber.second; y++)
    for (int x = 0; x < _patchNumber.first; x++) {
      int patchX0 = (float)x / _patchNumber.first * width;
      int patchY0 = (float)y / _patchNumber.second * height;
      int patchX1 = (float)(x + 1) / _patchNumber.first * width;
      int patchY1 = (float)(y + 1) / _patchNumber.second * height;

      int heightLevel = 0;
      for (int i = patchY0; i < patchY1; i++) {
        for (int j = patchX0; j < patchX1; j++) {
          int textureCoord = (j + i * width) * _heightMapCPU->getChannels();
          heightLevel = std::max(heightLevel, (int)_heightMapCPU->getData()[textureCoord]);
        }
      }

      int index = x + y * _patchNumber.first;
      if (heightLevel < _heightLevels[0]) {
        _patchTextures[index] = 0;
      } else if (heightLevel < _heightLevels[1]) {
        _patchTextures[index] = 1;
      } else if (heightLevel < _heightLevels[2]) {
        _patchTextures[index] = 2;
      } else {
        _patchTextures[index] = 3;
      }
    }
}

void TerrainComposition::_fillPatchRotationsDescription() {
  _patchRotationsIndex = std::vector<int>(_patchNumber.first * _patchNumber.second, 0);
}

void TerrainComposition::_allocatePatchDescription(int currentFrame) {
  _patchDescriptionSSBO[currentFrame] = std::make_shared<Buffer>(
      _patchNumber.first * _patchNumber.second * sizeof(PatchDescription), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);
}

void TerrainComposition::_updatePatchDescription(int currentFrame) {
  std::vector<PatchDescription> patchData(_patchNumber.first * _patchNumber.second);
  for (int i = 0; i < patchData.size(); i++) {
    patchData[i] = PatchDescription({.rotation = 90 * _patchRotationsIndex[i], .textureID = _patchTextures[i]});
  }
  // fill only number of lights because it's stub
  _patchDescriptionSSBO[currentFrame]->setData(patchData.data(), patchData.size() * sizeof(PatchDescription));
}

void TerrainComposition::_updateColorDescriptor() {
  int currentFrame = _engineState->getFrameInFlight();
  auto material = std::dynamic_pointer_cast<MaterialColor>(_material);
  std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
  std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
  std::vector<VkDescriptorBufferInfo> bufferInfoCameraControl(1);
  bufferInfoCameraControl[0].buffer = _cameraBuffer->getBuffer()[currentFrame]->getData();
  bufferInfoCameraControl[0].offset = 0;
  bufferInfoCameraControl[0].range = sizeof(BufferMVP);
  bufferInfoColor[0] = bufferInfoCameraControl;

  std::vector<VkDescriptorBufferInfo> bufferPatchInfo(1);
  bufferPatchInfo[0].buffer = _patchDescriptionSSBO[currentFrame]->getData();
  bufferPatchInfo[0].offset = 0;
  bufferPatchInfo[0].range = _patchDescriptionSSBO[currentFrame]->getSize();
  bufferInfoColor[1] = bufferPatchInfo;

  std::vector<VkDescriptorBufferInfo> bufferInfoCameraEval(1);
  bufferInfoCameraEval[0].buffer = _cameraBuffer->getBuffer()[currentFrame]->getData();
  bufferInfoCameraEval[0].offset = 0;
  bufferInfoCameraEval[0].range = sizeof(BufferMVP);
  bufferInfoColor[2] = bufferInfoCameraEval;

  std::vector<VkDescriptorImageInfo> textureHeightmap(1);
  textureHeightmap[0].imageLayout = _heightMap->getImageView()->getImage()->getImageLayout();
  textureHeightmap[0].imageView = _heightMap->getImageView()->getImageView();
  textureHeightmap[0].sampler = _heightMap->getSampler()->getSampler();
  textureInfoColor[3] = textureHeightmap;

  std::vector<VkDescriptorImageInfo> textureBaseColor(material->getBaseColor().size());
  for (int j = 0; j < material->getBaseColor().size(); j++) {
    textureBaseColor[j].imageLayout = material->getBaseColor()[j]->getImageView()->getImage()->getImageLayout();
    textureBaseColor[j].imageView = material->getBaseColor()[j]->getImageView()->getImageView();
    textureBaseColor[j].sampler = material->getBaseColor()[j]->getSampler()->getSampler();
  }
  textureInfoColor[4] = textureBaseColor;

  _descriptorSetColor->createCustom(currentFrame, bufferInfoColor, textureInfoColor);
}

void TerrainComposition::_updatePhongDescriptor() {
  int currentFrame = _engineState->getFrameInFlight();
  auto material = std::dynamic_pointer_cast<MaterialPhong>(_material);
  std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
  std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
  std::vector<VkDescriptorBufferInfo> bufferInfoCameraControl(1);
  bufferInfoCameraControl[0].buffer = _cameraBuffer->getBuffer()[currentFrame]->getData();
  bufferInfoCameraControl[0].offset = 0;
  bufferInfoCameraControl[0].range = sizeof(BufferMVP);
  bufferInfoColor[0] = bufferInfoCameraControl;

  std::vector<VkDescriptorBufferInfo> bufferPatchInfo(1);
  bufferPatchInfo[0].buffer = _patchDescriptionSSBO[currentFrame]->getData();
  bufferPatchInfo[0].offset = 0;
  bufferPatchInfo[0].range = _patchDescriptionSSBO[currentFrame]->getSize();
  bufferInfoColor[1] = bufferPatchInfo;

  std::vector<VkDescriptorBufferInfo> bufferInfoCameraEval(1);
  bufferInfoCameraEval[0].buffer = _cameraBuffer->getBuffer()[currentFrame]->getData();
  bufferInfoCameraEval[0].offset = 0;
  bufferInfoCameraEval[0].range = sizeof(BufferMVP);
  bufferInfoColor[2] = bufferInfoCameraEval;

  std::vector<VkDescriptorImageInfo> textureHeightmap(1);
  textureHeightmap[0].imageLayout = _heightMap->getImageView()->getImage()->getImageLayout();
  textureHeightmap[0].imageView = _heightMap->getImageView()->getImageView();
  textureHeightmap[0].sampler = _heightMap->getSampler()->getSampler();
  textureInfoColor[3] = textureHeightmap;

  std::vector<VkDescriptorImageInfo> textureBaseColor(material->getBaseColor().size());
  for (int j = 0; j < material->getBaseColor().size(); j++) {
    textureBaseColor[j].imageLayout = material->getBaseColor()[j]->getImageView()->getImage()->getImageLayout();
    textureBaseColor[j].imageView = material->getBaseColor()[j]->getImageView()->getImageView();
    textureBaseColor[j].sampler = material->getBaseColor()[j]->getSampler()->getSampler();
  }
  textureInfoColor[4] = textureBaseColor;

  std::vector<VkDescriptorImageInfo> textureBaseNormal(material->getNormal().size());
  for (int j = 0; j < material->getNormal().size(); j++) {
    textureBaseNormal[j].imageLayout = material->getNormal()[j]->getImageView()->getImage()->getImageLayout();
    textureBaseNormal[j].imageView = material->getNormal()[j]->getImageView()->getImageView();
    textureBaseNormal[j].sampler = material->getNormal()[j]->getSampler()->getSampler();
  }
  textureInfoColor[5] = textureBaseNormal;

  std::vector<VkDescriptorImageInfo> textureBaseSpecular(material->getSpecular().size());
  for (int j = 0; j < material->getSpecular().size(); j++) {
    textureBaseSpecular[j].imageLayout = material->getSpecular()[j]->getImageView()->getImage()->getImageLayout();
    textureBaseSpecular[j].imageView = material->getSpecular()[j]->getImageView()->getImageView();
    textureBaseSpecular[j].sampler = material->getSpecular()[j]->getSampler()->getSampler();
  }
  textureInfoColor[6] = textureBaseSpecular;

  std::vector<VkDescriptorBufferInfo> bufferInfoCoefficients(1);
  // write to binding = 0 for vertex shader
  bufferInfoCoefficients[0].buffer = material->getBufferCoefficients()->getBuffer()[currentFrame]->getData();
  bufferInfoCoefficients[0].offset = 0;
  bufferInfoCoefficients[0].range = material->getBufferCoefficients()->getBuffer()[currentFrame]->getSize();
  bufferInfoColor[7] = bufferInfoCoefficients;
  _descriptorSetPhong->createCustom(currentFrame, bufferInfoColor, textureInfoColor);
}

void TerrainComposition::_updatePBRDescriptor() {
  int currentFrame = _engineState->getFrameInFlight();
  auto material = std::dynamic_pointer_cast<MaterialPBR>(_material);
  std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
  std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
  std::vector<VkDescriptorBufferInfo> bufferInfoCameraControl(1);
  bufferInfoCameraControl[0].buffer = _cameraBuffer->getBuffer()[currentFrame]->getData();
  bufferInfoCameraControl[0].offset = 0;
  bufferInfoCameraControl[0].range = sizeof(BufferMVP);
  bufferInfoColor[0] = bufferInfoCameraControl;

  std::vector<VkDescriptorBufferInfo> bufferPatchInfo(1);
  bufferPatchInfo[0].buffer = _patchDescriptionSSBO[currentFrame]->getData();
  bufferPatchInfo[0].offset = 0;
  bufferPatchInfo[0].range = _patchDescriptionSSBO[currentFrame]->getSize();
  bufferInfoColor[1] = bufferPatchInfo;

  std::vector<VkDescriptorBufferInfo> bufferInfoCameraEval(1);
  bufferInfoCameraEval[0].buffer = _cameraBuffer->getBuffer()[currentFrame]->getData();
  bufferInfoCameraEval[0].offset = 0;
  bufferInfoCameraEval[0].range = sizeof(BufferMVP);
  bufferInfoColor[2] = bufferInfoCameraEval;

  std::vector<VkDescriptorImageInfo> textureHeightmap(1);
  textureHeightmap[0].imageLayout = _heightMap->getImageView()->getImage()->getImageLayout();
  textureHeightmap[0].imageView = _heightMap->getImageView()->getImageView();
  textureHeightmap[0].sampler = _heightMap->getSampler()->getSampler();
  textureInfoColor[3] = textureHeightmap;

  std::vector<VkDescriptorImageInfo> textureBaseColor(material->getBaseColor().size());
  for (int j = 0; j < material->getBaseColor().size(); j++) {
    textureBaseColor[j].imageLayout = material->getBaseColor()[j]->getImageView()->getImage()->getImageLayout();
    textureBaseColor[j].imageView = material->getBaseColor()[j]->getImageView()->getImageView();
    textureBaseColor[j].sampler = material->getBaseColor()[j]->getSampler()->getSampler();
  }
  textureInfoColor[4] = textureBaseColor;

  std::vector<VkDescriptorImageInfo> textureBaseNormal(material->getNormal().size());
  for (int j = 0; j < material->getNormal().size(); j++) {
    textureBaseNormal[j].imageLayout = material->getNormal()[j]->getImageView()->getImage()->getImageLayout();
    textureBaseNormal[j].imageView = material->getNormal()[j]->getImageView()->getImageView();
    textureBaseNormal[j].sampler = material->getNormal()[j]->getSampler()->getSampler();
  }
  textureInfoColor[5] = textureBaseNormal;

  std::vector<VkDescriptorImageInfo> textureBaseMetallic(material->getMetallic().size());
  for (int j = 0; j < material->getMetallic().size(); j++) {
    textureBaseMetallic[j].imageLayout = material->getMetallic()[j]->getImageView()->getImage()->getImageLayout();
    textureBaseMetallic[j].imageView = material->getMetallic()[j]->getImageView()->getImageView();
    textureBaseMetallic[j].sampler = material->getMetallic()[j]->getSampler()->getSampler();
  }
  textureInfoColor[6] = textureBaseMetallic;

  std::vector<VkDescriptorImageInfo> textureBaseRoughness(material->getRoughness().size());
  for (int j = 0; j < material->getRoughness().size(); j++) {
    textureBaseRoughness[j].imageLayout = material->getRoughness()[j]->getImageView()->getImage()->getImageLayout();
    textureBaseRoughness[j].imageView = material->getRoughness()[j]->getImageView()->getImageView();
    textureBaseRoughness[j].sampler = material->getRoughness()[j]->getSampler()->getSampler();
  }
  textureInfoColor[7] = textureBaseRoughness;

  std::vector<VkDescriptorImageInfo> textureBaseOcclusion(material->getOccluded().size());
  for (int j = 0; j < material->getOccluded().size(); j++) {
    textureBaseOcclusion[j].imageLayout = material->getOccluded()[j]->getImageView()->getImage()->getImageLayout();
    textureBaseOcclusion[j].imageView = material->getOccluded()[j]->getImageView()->getImageView();
    textureBaseOcclusion[j].sampler = material->getOccluded()[j]->getSampler()->getSampler();
  }
  textureInfoColor[8] = textureBaseOcclusion;

  std::vector<VkDescriptorImageInfo> textureBaseEmissive(material->getEmissive().size());
  for (int j = 0; j < material->getEmissive().size(); j++) {
    textureBaseEmissive[j].imageLayout = material->getEmissive()[j]->getImageView()->getImage()->getImageLayout();
    textureBaseEmissive[j].imageView = material->getEmissive()[j]->getImageView()->getImageView();
    textureBaseEmissive[j].sampler = material->getEmissive()[j]->getSampler()->getSampler();
  }
  textureInfoColor[9] = textureBaseEmissive;

  // TODO: this textures are part of global engineState for PBR
  std::vector<VkDescriptorImageInfo> bufferInfoIrradiance(1);
  bufferInfoIrradiance[0].imageLayout = material->getDiffuseIBL()->getImageView()->getImage()->getImageLayout();
  bufferInfoIrradiance[0].imageView = material->getDiffuseIBL()->getImageView()->getImageView();
  bufferInfoIrradiance[0].sampler = material->getDiffuseIBL()->getSampler()->getSampler();
  textureInfoColor[10] = bufferInfoIrradiance;

  std::vector<VkDescriptorImageInfo> bufferInfoSpecularIBL(1);
  bufferInfoSpecularIBL[0].imageLayout = material->getSpecularIBL()->getImageView()->getImage()->getImageLayout();
  bufferInfoSpecularIBL[0].imageView = material->getSpecularIBL()->getImageView()->getImageView();
  bufferInfoSpecularIBL[0].sampler = material->getSpecularIBL()->getSampler()->getSampler();
  textureInfoColor[11] = bufferInfoSpecularIBL;

  std::vector<VkDescriptorImageInfo> bufferInfoSpecularBRDF(1);
  bufferInfoSpecularBRDF[0].imageLayout = material->getSpecularBRDF()->getImageView()->getImage()->getImageLayout();
  bufferInfoSpecularBRDF[0].imageView = material->getSpecularBRDF()->getImageView()->getImageView();
  bufferInfoSpecularBRDF[0].sampler = material->getSpecularBRDF()->getSampler()->getSampler();
  textureInfoColor[12] = bufferInfoSpecularBRDF;

  std::vector<VkDescriptorBufferInfo> bufferInfoCoefficients(1);
  // write to binding = 0 for vertex shader
  bufferInfoCoefficients[0].buffer = material->getBufferCoefficients()->getBuffer()[currentFrame]->getData();
  bufferInfoCoefficients[0].offset = 0;
  bufferInfoCoefficients[0].range = material->getBufferCoefficients()->getBuffer()[currentFrame]->getSize();
  bufferInfoColor[13] = bufferInfoCoefficients;

  std::vector<VkDescriptorBufferInfo> bufferInfoAlphaCutoff(1);
  // write to binding = 0 for vertex shader
  bufferInfoAlphaCutoff[0].buffer = material->getBufferAlphaCutoff()->getBuffer()[currentFrame]->getData();
  bufferInfoAlphaCutoff[0].offset = 0;
  bufferInfoAlphaCutoff[0].range = material->getBufferAlphaCutoff()->getBuffer()[currentFrame]->getSize();
  bufferInfoColor[14] = bufferInfoAlphaCutoff;

  _descriptorSetPBR->createCustom(currentFrame, bufferInfoColor, textureInfoColor);
}

void TerrainComposition::draw(std::shared_ptr<CommandBuffer> commandBuffer) {
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

    if (pipeline->getPushConstants().find("control") != pipeline->getPushConstants().end()) {
      TesselationControlPush pushConstants;
      pushConstants.patchDimX = _patchNumber.first;
      pushConstants.patchDimY = _patchNumber.second;
      pushConstants.minTesselationDistance = _minTesselationDistance;
      pushConstants.maxTesselationDistance = _maxTesselationDistance;
      pushConstants.minTessellationLevel = _minTessellationLevel;
      pushConstants.maxTessellationLevel = _maxTessellationLevel;
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0, sizeof(TesselationControlPush), &pushConstants);
    }

    if (pipeline->getPushConstants().find("evaluate") != pipeline->getPushConstants().end()) {
      TesselationEvaluationPush pushConstants;
      pushConstants.patchDimX = _patchNumber.first;
      pushConstants.patchDimY = _patchNumber.second;
      pushConstants.heightScale = _heightScale;
      pushConstants.heightShift = _heightShift;
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, sizeof(TesselationControlPush),
                         sizeof(TesselationEvaluationPush), &pushConstants);
    }

    if (pipeline->getPushConstants().find("evaluateDepth") != pipeline->getPushConstants().end()) {
      TesselationEvaluationPushDepth pushConstants;
      pushConstants.heightScale = _heightScale;
      pushConstants.heightShift = _heightShift;
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, sizeof(TesselationControlPush),
                         sizeof(TesselationEvaluationPushDepth), &pushConstants);
    }

    if (pipeline->getPushConstants().find("fragment") != pipeline->getPushConstants().end()) {
      FragmentPush pushConstants;
      pushConstants.enableShadow = _enableShadow;
      pushConstants.enableLighting = _enableLighting;
      pushConstants.cameraPosition = _gameState->getCameraManager()->getCurrentCamera()->getEye();
      vkCmdPushConstants(
          commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT,
          sizeof(TesselationControlPush) + sizeof(TesselationEvaluationPush), sizeof(FragmentPush), &pushConstants);
    }

    if (pipeline->getPushConstants().find("fragmentColor") != pipeline->getPushConstants().end()) {
      FragmentPush pushConstants;
      pushConstants.enableShadow = _enableShadow;
      pushConstants.enableLighting = _enableLighting;
      pushConstants.cameraPosition = _gameState->getCameraManager()->getCurrentCamera()->getEye();
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_FRAGMENT_BIT,
                         sizeof(TesselationControlPush) + sizeof(TesselationEvaluationPushDepth), sizeof(FragmentPush),
                         &pushConstants);
    }

    // same buffer to both tessellation shaders because we're not going to change camera between these 2 stages
    BufferMVP cameraUBO{};
    cameraUBO.model = getModel();
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

  auto pipeline = _pipeline[_materialType];
  drawTerrain(pipeline);
}

void TerrainComposition::drawShadow(LightType lightType,
                                    int lightIndex,
                                    int face,
                                    std::shared_ptr<CommandBuffer> commandBuffer) {
  int currentFrame = _engineState->getFrameInFlight();
  auto pipeline = _pipelineDirectional;
  if (lightType == LightType::POINT) pipeline = _pipelinePoint;

  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->getPipeline());

  std::tuple<int, int> resolution = _engineState->getSettings()->getShadowMapResolution();

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

  glm::mat4 view(1.f);
  glm::mat4 projection(1.f);
  float far;
  int lightIndexTotal = lightIndex;
  if (lightType == LightType::DIRECTIONAL) {
    view = _gameState->getLightManager()->getDirectionalLights()[lightIndex]->getCamera()->getView();
    projection = _gameState->getLightManager()->getDirectionalLights()[lightIndex]->getCamera()->getProjection();
    far = _gameState->getLightManager()->getDirectionalLights()[lightIndex]->getCamera()->getFar();
  }
  if (lightType == LightType::POINT) {
    lightIndexTotal += _engineState->getSettings()->getMaxDirectionalLights();
    view = _gameState->getLightManager()->getPointLights()[lightIndex]->getCamera()->getView(face);
    projection = _gameState->getLightManager()->getPointLights()[lightIndex]->getCamera()->getProjection();
    far = _gameState->getLightManager()->getPointLights()[lightIndex]->getCamera()->getFar();
  }

  if (pipeline->getPushConstants().find("controlDepth") != pipeline->getPushConstants().end()) {
    TesselationControlPushDepth pushConstants;
    pushConstants.minTesselationDistance = _minTesselationDistance;
    pushConstants.maxTesselationDistance = _maxTesselationDistance;
    pushConstants.minTessellationLevel = _minTessellationLevel;
    pushConstants.maxTessellationLevel = _maxTessellationLevel;
    vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                       VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0, sizeof(TesselationControlPushDepth),
                       &pushConstants);
  }

  if (pipeline->getPushConstants().find("evaluateDepth") != pipeline->getPushConstants().end()) {
    TesselationEvaluationPushDepth pushConstants;
    pushConstants.heightScale = _heightScale;
    pushConstants.heightShift = _heightShift;
    vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                       VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, sizeof(TesselationControlPushDepth),
                       sizeof(TesselationEvaluationPushDepth), &pushConstants);
  }

  if (pipeline->getPushConstants().find("fragmentDepth") != pipeline->getPushConstants().end()) {
    DepthConstants pushConstants;
    pushConstants.lightPosition =
        _gameState->getLightManager()->getPointLights()[lightIndex]->getCamera()->getPosition();
    // light camera
    pushConstants.far = far;
    vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                       VK_SHADER_STAGE_FRAGMENT_BIT, pipeline->getPushConstants()["fragmentDepth"].offset,
                       pipeline->getPushConstants()["fragmentDepth"].size, &pushConstants);
  }

  // same buffer to both tessellation shaders because we're not going to change camera between these 2 stages
  BufferMVP cameraUBO{};
  cameraUBO.model = getModel();
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