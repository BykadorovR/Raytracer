#include "Sprite.h"

struct UniformObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 projection;
};

Sprite::Sprite(std::shared_ptr<Texture> texture,
               std::shared_ptr<DescriptorSetLayout> descriptorSetLayout,
               std::shared_ptr<Pipeline> pipeline,
               std::shared_ptr<DescriptorPool> descriptorPool,
               std::shared_ptr<CommandPool> commandPool,
               std::shared_ptr<CommandBuffer> commandBuffer,
               std::shared_ptr<Queue> queue,
               std::shared_ptr<Device> device,
               std::shared_ptr<Settings> settings) {
  _pipeline = pipeline;
  _commandBuffer = commandBuffer;
  _device = device;
  _settings = settings;
  _texture = texture;

  _vertexBuffer = std::make_shared<VertexBuffer2D>(_vertices, commandPool, queue, device);
  _indexBuffer = std::make_shared<IndexBuffer>(_indices, commandPool, queue, device);
  _uniformBuffer = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(UniformObject), commandPool,
                                                   queue, device);
  _descriptorSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), descriptorSetLayout,
                                                   descriptorPool, device);
  _descriptorSet->createGraphic(texture, _uniformBuffer);

  _model = glm::mat4(1.f);
  _view = glm::mat4(1.f);
  _projection = glm::mat4(1.f);
}

void Sprite::setModel(glm::mat4 model) { _model = model; }

void Sprite::setView(glm::mat4 view) { _view = view; }

void Sprite::setProjection(glm::mat4 projection) { _projection = projection; }

void Sprite::draw(int currentFrame) {
  UniformObject ubo{};
  ubo.model = _model;
  ubo.view = _view;
  ubo.projection = _projection;

  void* data;
  vkMapMemory(_device->getLogicalDevice(), _uniformBuffer->getBuffer()[currentFrame]->getMemory(), 0, sizeof(ubo), 0,
              &data);
  memcpy(data, &ubo, sizeof(ubo));
  vkUnmapMemory(_device->getLogicalDevice(), _uniformBuffer->getBuffer()[currentFrame]->getMemory());

  VkBuffer vertexBuffers[] = {_vertexBuffer->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(_commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(_commandBuffer->getCommandBuffer()[currentFrame], _indexBuffer->getBuffer()->getData(), 0,
                       VK_INDEX_TYPE_UINT32);
  vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                          _pipeline->getPipelineLayout(), 0, 1, &_descriptorSet->getDescriptorSets()[currentFrame], 0,
                          nullptr);

  vkCmdDrawIndexed(_commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_indices.size()), 1, 0, 0,
                   0);
}