#include "Sprite.h"

const std::vector<Vertex> vertices = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                      {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                      {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                      {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};
const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};
struct UniformObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

Sprite::Sprite(std::shared_ptr<DescriptorPool> descriptorPool,
               std::shared_ptr<CommandPool> commandPool,
               std::shared_ptr<CommandBuffer> commandBuffer,
               std::shared_ptr<Queue> queue,
               std::shared_ptr<RenderPass> render,
               std::shared_ptr<Device> device,
               std::shared_ptr<Settings> settings) {
  _settings = settings;
  _device = device;
  _render = render;
  _queue = queue;
  _commandPool = commandPool;
  _descriptorPool = descriptorPool;
  _commandBuffer = commandBuffer;

  _descriptorSetLayout = std::make_shared<DescriptorSetLayout>(device);
  _shader = std::make_shared<Shader>("../shaders/simple_vertex.spv", "../shaders/simple_fragment.spv", device);
  _pipeline = std::make_shared<Pipeline>(_shader, _descriptorSetLayout, _render, device);

  _vertexBuffer = std::make_shared<VertexBuffer>(vertices, commandPool, queue, device);

  _indexBuffer = std::make_shared<IndexBuffer>(indices, commandPool, queue, device);

  _uniformBuffer = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(UniformObject), commandPool,
                                                   queue, device);
  _descriptorSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), 0, _uniformBuffer,
                                                   _descriptorSetLayout, descriptorPool, device);
}

void Sprite::draw(int currentFrame, glm::mat4 model, glm::mat4 view, glm::mat4 proj) {
  UniformObject ubo{};
  ubo.model = model;
  ubo.view = view;
  ubo.proj = proj;

  vkCmdBindPipeline(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline->getPipeline());

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  std::tie(viewport.width, viewport.height) = _settings->getResolution();
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(_commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(viewport.width, viewport.height);
  vkCmdSetScissor(_commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  void* data;
  vkMapMemory(_device->getLogicalDevice(), _uniformBuffer->getBuffer()[currentFrame]->getMemory(), 0, sizeof(ubo), 0,
              &data);
  memcpy(data, &ubo, sizeof(ubo));
  vkUnmapMemory(_device->getLogicalDevice(), _uniformBuffer->getBuffer()[currentFrame]->getMemory());

  VkBuffer vertexBuffers[] = {_vertexBuffer->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(_commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(_commandBuffer->getCommandBuffer()[currentFrame], _indexBuffer->getBuffer()->getData(), 0,
                       VK_INDEX_TYPE_UINT16);
  vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                          _pipeline->getPipelineLayout(), 0, 1, &_descriptorSet->getDescriptorSets()[currentFrame], 0,
                          nullptr);

  vkCmdDrawIndexed(_commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
}