#include "Primitive/Line.h"

Line::Line(std::shared_ptr<CommandBuffer> commandBufferTransfer,
           std::shared_ptr<GameState> gameState,
           std::shared_ptr<EngineState> engineState) {
  setName("Line");
  _engineState = engineState;
  _gameState = gameState;
  _mesh = std::make_shared<MeshDynamic3D>(engineState);
  _mesh->setIndexes({0, 1});
  _mesh->setVertices({Vertex3D{}, Vertex3D{}});
  _uniformBuffer.resize(_engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _uniformBuffer[i] = std::make_shared<Buffer>(
        sizeof(BufferMVP), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, engineState);
  }

  auto setLayout = std::make_shared<DescriptorSetLayout>(engineState->getDevice());
  setLayout->createUniformBuffer();
  _descriptorSetCamera = std::make_shared<DescriptorSet>(engineState->getSettings()->getMaxFramesInFlight(), setLayout,
                                                         engineState->getDescriptorPool(), engineState->getDevice());
  _descriptorSetCamera->createUniformBuffer(_uniformBuffer);

  auto shader = std::make_shared<Shader>(engineState);
  shader->add("shaders/line/line_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader->add("shaders/line/line_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  _renderPass = std::make_shared<RenderPass>(_engineState->getSettings(), _engineState->getDevice());
  _renderPass->initializeGraphic();
  _pipeline = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());
  _pipeline->createGeometry(
      VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
      {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
       shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
      {std::pair{std::string("camera"), setLayout}}, {}, _mesh->getBindingDescription(),
      _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                             {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)}}),
      _renderPass);
}

std::shared_ptr<MeshDynamic3D> Line::getMesh() { return _mesh; }

void Line::setModel(glm::mat4 model) { _model = model; }

void Line::draw(std::shared_ptr<CommandBuffer> commandBuffer) {
  auto currentFrame = _engineState->getFrameInFlight();
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline->getPipeline());
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

  BufferMVP cameraUBO{};
  cameraUBO.model = _model;
  cameraUBO.view = _gameState->getCameraManager()->getCurrentCamera()->getView();
  cameraUBO.projection = _gameState->getCameraManager()->getCurrentCamera()->getProjection();

  _uniformBuffer[currentFrame]->setData(&cameraUBO);

  VkBuffer vertexBuffers[] = {_mesh->getVertexBuffer()->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame], _mesh->getIndexBuffer()->getBuffer()->getData(),
                       0, VK_INDEX_TYPE_UINT32);

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

  vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_mesh->getIndexData().size()),
                   1, 0, 0, 0);
}