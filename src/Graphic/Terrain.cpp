#include "Terrain.h"

struct UniformObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 projection;
};

Terrain::Terrain(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state) {
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
      vertex.pos = glm::vec3(-height / 2.0f + i, (int)y * yScale - yShift, -width / 2.0f + j);
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

  _uniformBuffer = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(UniformObject),
                                                   state->getDevice());
  auto setLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
  setLayout->createCamera();
  _descriptorSetCamera = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(), setLayout,
                                                         state->getDescriptorPool(), state->getDevice());
  _descriptorSetCamera->createCamera(_uniformBuffer);

  auto shader = std::make_shared<Shader>(state->getDevice());
  shader->add("../shaders/sphere_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader->add("../shaders/sphere_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  _pipeline = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _pipeline->createGraphicTerrain(VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
                                  {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                   shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                  {std::pair{std::string("camera"), setLayout}}, {}, Vertex3D::getBindingDescription(),
                                  Vertex3D::getAttributeDescriptions());
}

void Terrain::setModel(glm::mat4 model) { _model = model; }

void Terrain::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

void Terrain::draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer) {
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

  UniformObject cameraUBO{};
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
    vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_indices.size()), 1, 0,
                     strip * _numVertsPerStrip, 0);
  }
}