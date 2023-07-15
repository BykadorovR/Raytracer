#include "Terrain.h"

struct CameraObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 projection;
};

void Terrain::setModel(glm::mat4 model) { _model = model; }

void Terrain::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

TerrainCPU::TerrainCPU(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state) {
  _state = state;

  int width, height, channels;
  unsigned char* data = stbi_load("../data/heightmap.png", &width, &height, &channels, 0);

  // vertex generation
  std::vector<Vertex3D> vertices;
  float yScale = 64.0f / 256.0f, yShift = 16.0f;  // apply a scale+shift to the height data
  for (unsigned int i = 0; i < height; i++) {
    for (unsigned int j = 0; j < width; j++) {
      // retrieve texel for (i,j) tex coord
      unsigned char* texel = data + (j + width * i) * channels;
      // raw height at coordinate
      unsigned char y = texel[0];

      // vertex
      Vertex3D vertex;
      vertex.pos = glm::vec3(-width / 2.0f + j, (int)y * yScale - yShift, -height / 2.0f + i);
      vertices.push_back(vertex);
    }
  }

  stbi_image_free(data);

  // index generation
  for (unsigned int i = 0; i < height - 1; i++)  // for each row a.k.a. each strip
  {
    for (unsigned int j = 0; j < width; j++)  // for each column
    {
      for (unsigned int k = 0; k < 2; k++)  // for each side of the strip
      {
        _indices.push_back(j + width * (i + k));
      }
    }
  }

  _numStrips = height - 1;
  _numVertsPerStrip = width * 2;

  _vertexBuffer = std::make_shared<VertexBuffer3D>(vertices, commandBufferTransfer, state->getDevice());
  _indexBuffer = std::make_shared<IndexBuffer>(_indices, commandBufferTransfer, state->getDevice());

  _uniformBuffer = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(CameraObject),
                                                   state->getDevice());
  auto setLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
  setLayout->createBuffer();
  _descriptorSetCamera = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(), setLayout,
                                                         state->getDescriptorPool(), state->getDevice());
  _descriptorSetCamera->createBuffer(_uniformBuffer);

  auto shader = std::make_shared<Shader>(state->getDevice());
  shader->add("../shaders/terrainCPU_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader->add("../shaders/terrainCPU_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  _pipeline = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _pipeline->createGraphicTerrainCPU(VK_CULL_MODE_NONE, VK_POLYGON_MODE_LINE,
                                     {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                      shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                     {std::pair{std::string("camera"), setLayout}}, {},
                                     Vertex3D::getBindingDescription(), Vertex3D::getAttributeDescriptions());
}

void TerrainCPU::draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer) {
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline->getPipeline());
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

  CameraObject cameraUBO{};
  cameraUBO.model = _model;
  cameraUBO.view = _camera->getView();
  cameraUBO.projection = _camera->getProjection();

  void* data;
  vkMapMemory(_state->getDevice()->getLogicalDevice(), _uniformBuffer->getBuffer()[currentFrame]->getMemory(), 0,
              sizeof(cameraUBO), 0, &data);
  memcpy(data, &cameraUBO, sizeof(cameraUBO));
  vkUnmapMemory(_state->getDevice()->getLogicalDevice(), _uniformBuffer->getBuffer()[currentFrame]->getMemory());

  VkBuffer vertexBuffers[] = {_vertexBuffer->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame], _indexBuffer->getBuffer()->getData(), 0,
                       VK_INDEX_TYPE_UINT32);

  auto pipelineLayout = _pipeline->getDescriptorSetLayout();
  auto cameraLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                   [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                     return info.first == std::string("camera");
                                   });
  if (cameraLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline->getPipelineLayout(), 0, 1,
                            &_descriptorSetCamera->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  for (unsigned int strip = 0; strip < _numStrips; ++strip) {
    vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], _numVertsPerStrip, 1, strip * _numVertsPerStrip,
                     0, 0);
  }
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

TerrainGPU::TerrainGPU(int patchSize,
                       std::shared_ptr<CommandBuffer> commandBufferTransfer,
                       std::shared_ptr<State> state) {
  _state = state;

  _heightMap = std::make_shared<Texture>("../data/heightmap.png", VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                         commandBufferTransfer, state->getDevice());
  auto [width, height] = _heightMap->getImageView()->getImage()->getResolution();
  // vertex generation
  std::vector<Vertex3D> vertices;
  for (unsigned i = 0; i <= patchSize - 1; i++) {
    for (unsigned j = 0; j <= patchSize - 1; j++) {
      Vertex3D vertex1;
      vertex1.pos = glm::vec3(-width / 2.0f + width * i / (float)patchSize, 0.f,
                              -height / 2.0f + height * j / (float)patchSize);
      vertex1.texCoord = glm::vec2(i / (float)patchSize, j / (float)patchSize);
      vertices.push_back(vertex1);

      Vertex3D vertex2;
      vertex2.pos = glm::vec3(-width / 2.0f + width * (i + 1) / (float)patchSize, 0.f,
                              -height / 2.0f + height * j / (float)patchSize);
      vertex2.texCoord = glm::vec2((i + 1) / (float)patchSize, j / (float)patchSize);
      vertices.push_back(vertex2);

      Vertex3D vertex3;
      vertex3.pos = glm::vec3(-width / 2.0f + width * i / (float)patchSize, 0.f,
                              -height / 2.0f + height * (j + 1) / (float)patchSize);
      vertex3.texCoord = glm::vec2(i / (float)patchSize, (j + 1) / (float)patchSize);
      vertices.push_back(vertex3);

      Vertex3D vertex4;
      vertex4.pos = glm::vec3(-width / 2.0f + width * (i + 1) / (float)patchSize, 0.f,
                              -height / 2.0f + height * (j + 1) / (float)patchSize);
      vertex4.texCoord = glm::vec2((i + 1) / (float)patchSize, (j + 1) / (float)patchSize);
      vertices.push_back(vertex4);
    }
  }

  _vertexNumber = vertices.size();
  _vertexBuffer = std::make_shared<VertexBuffer3D>(vertices, commandBufferTransfer, state->getDevice());
  _cameraBuffer = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(CameraObject),
                                                  state->getDevice());

  auto setCameraControl = std::make_shared<DescriptorSetLayout>(state->getDevice());
  setCameraControl->createBuffer(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);

  auto setCameraEvaluation = std::make_shared<DescriptorSetLayout>(state->getDevice());
  setCameraEvaluation->createBuffer(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

  auto setHeight = std::make_shared<DescriptorSetLayout>(state->getDevice());
  setHeight->createTexture(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

  _descriptorSetCameraControl = std::make_shared<DescriptorSet>(
      state->getSettings()->getMaxFramesInFlight(), setCameraControl, state->getDescriptorPool(), state->getDevice());
  _descriptorSetCameraControl->createBuffer(_cameraBuffer);

  _descriptorSetCameraEvaluation = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                                   setCameraEvaluation, state->getDescriptorPool(),
                                                                   state->getDevice());
  _descriptorSetCameraEvaluation->createBuffer(_cameraBuffer);

  _descriptorSetHeight = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(), setHeight,
                                                         state->getDescriptorPool(), state->getDevice());
  _descriptorSetHeight->createTexture(_heightMap);

  auto shader = std::make_shared<Shader>(state->getDevice());
  shader->add("../shaders/terrainGPU_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader->add("../shaders/terrainGPU_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  shader->add("../shaders/terrainGPU_control.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
  shader->add("../shaders/terrainGPU_evaluation.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

  std::map<std::string, VkPushConstantRange> defaultPushConstants;
  defaultPushConstants["control"] = LoDConstants::getPushConstant();

  _pipeline = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _pipeline->createGraphicTerrainGPU(
      VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
      {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT), shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
       shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT),
       shader->getShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)},
      {std::pair{std::string("cameraControl"), setCameraControl},
       std::pair{std::string("cameraEvaluation"), setCameraEvaluation}, std::pair{std::string("height"), setHeight}},
      defaultPushConstants, Vertex3D::getBindingDescription(), Vertex3D::getAttributeDescriptions());
}

void TerrainGPU::draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer) {
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline->getPipeline());
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

  if (_pipeline->getPushConstants().find("control") != _pipeline->getPushConstants().end()) {
    LoDConstants pushConstants;
    pushConstants.near = _minDistance;
    pushConstants.far = _maxDistance;
    pushConstants.minTessellationLevel = _minTessellationLevel;
    pushConstants.maxTessellationLevel = _maxTessellationLevel;

    vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], _pipeline->getPipelineLayout(),
                       VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 0, sizeof(LoDConstants), &pushConstants);
  }

  // same buffer to both tessellation shaders because we're not going to change camera between these 2 stages
  CameraObject cameraUBO{};
  cameraUBO.model = _model;
  cameraUBO.view = _camera->getView();
  cameraUBO.projection = _camera->getProjection();

  void* data;
  vkMapMemory(_state->getDevice()->getLogicalDevice(), _cameraBuffer->getBuffer()[currentFrame]->getMemory(), 0,
              sizeof(cameraUBO), 0, &data);
  memcpy(data, &cameraUBO, sizeof(cameraUBO));
  vkUnmapMemory(_state->getDevice()->getLogicalDevice(), _cameraBuffer->getBuffer()[currentFrame]->getMemory());

  VkBuffer vertexBuffers[] = {_vertexBuffer->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  auto pipelineLayout = _pipeline->getDescriptorSetLayout();
  {
    auto cameralLayoutControl = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                             [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                               return info.first == std::string("cameraControl");
                                             });
    if (cameralLayoutControl != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              _pipeline->getPipelineLayout(), 0, 1,
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
                              _pipeline->getPipelineLayout(), 1, 1,
                              &_descriptorSetCameraEvaluation->getDescriptorSets()[currentFrame], 0, nullptr);
    }
  }

  {
    auto heightLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                     [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                       return info.first == std::string("height");
                                     });
    if (heightLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              _pipeline->getPipelineLayout(), 2, 1,
                              &_descriptorSetHeight->getDescriptorSets()[currentFrame], 0, nullptr);
    }
  }

  vkCmdDraw(commandBuffer->getCommandBuffer()[currentFrame], _vertexNumber, 1, 0, 0);
}