#include "Line.h"

struct UniformObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 projection;
};

Line::Line(int thick,
           VkFormat renderFormat,
           std::shared_ptr<CommandBuffer> commandBufferTransfer,
           std::shared_ptr<State> state) {
  _state = state;
  _commandBufferTransfer = commandBufferTransfer;

  _indices.push_back(0);
  _indices.push_back(1);

  std::vector<Vertex3D> vertices;
  vertices.push_back(Vertex3D());
  vertices.push_back(Vertex3D());
  _stagingBuffer = std::make_shared<Buffer>(vertices.size() * sizeof(Vertex3D), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                            _state->getDevice());
  _vertexBuffer = std::make_shared<VertexBuffer<Vertex3D>>(vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                           _commandBufferTransfer, _state->getDevice());
  _indexBuffer = std::make_shared<VertexBuffer<uint32_t>>(_indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                          _commandBufferTransfer, _state->getDevice());
  _uniformBuffer = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(UniformObject),
                                                   state->getDevice());
  auto setLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
  setLayout->createUniformBuffer();
  _descriptorSetCamera = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(), setLayout,
                                                         state->getDescriptorPool(), state->getDevice());
  _descriptorSetCamera->createUniformBuffer(_uniformBuffer);

  auto shader = std::make_shared<Shader>(state->getDevice());
  shader->add("../shaders/sphere_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader->add("../shaders/sphere_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  _pipeline = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _pipeline->createLine(renderFormat, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, thick,
                        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                        {std::pair{std::string("camera"), setLayout}}, {}, Vertex3D::getBindingDescription(),
                        Vertex3D::getAttributeDescriptions());
}

void Line::setColor(glm::vec3 p0, glm::vec3 p1) {
  _color.first = p0;
  _color.second = p1;
  _changed = true;
}

std::pair<glm::vec3, glm::vec3> Line::getPosition() { return _position; }

void Line::setPosition(glm::vec3 p0, glm::vec3 p1) {
  _position.first = p0;
  _position.second = p1;
  _changed = true;
}

void Line::setModel(glm::mat4 model) { _model = model; }

void Line::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

void Line::draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer) {
  if (_changed) {
    _changed = false;
    std::vector<Vertex3D> vertices;
    Vertex3D vertex1;
    vertex1.pos = _position.first;
    vertex1.color = _color.first;
    vertices.push_back(vertex1);
    Vertex3D vertex2;
    vertex2.pos = _position.second;
    vertex2.color = _color.second;
    vertices.push_back(vertex2);
    // TODO: blocking operation
    _vertexBuffer->setData(vertices);
  }
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

  vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_indices.size()), 1, 0, 0, 0);
}