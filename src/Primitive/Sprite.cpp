#include "Sprite.h"

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

  _vertexBuffer = std::make_shared<VertexBuffer>(_vertices, commandPool, queue, device);
  _indexBuffer = std::make_shared<IndexBuffer>(_indices, commandPool, queue, device);
  _uniformBufferCamera = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(UniformObjectCamera),
                                                   commandPool, queue, device);
  _uniformBufferPointLight = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(UniformObjectPointLights), commandPool, queue,
                                                             device);
  _descriptorSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), descriptorSetLayout,
                                                   descriptorPool, device);
  _descriptorSet->createGraphic(texture, _uniformBufferCamera, _uniformBufferPointLight);

  _model = glm::mat4(1.f);
  _view = glm::mat4(1.f);
  _projection = glm::mat4(1.f);
}

void Sprite::setModel(glm::mat4 model) { _model = model; }

void Sprite::setView(glm::mat4 view) { _view = view; }

void Sprite::setProjection(glm::mat4 projection) { _projection = projection; }

void Sprite::draw(int currentFrame) {
  {
    UniformObjectCamera ubo{};
    ubo.model = _model;
    ubo.view = _view;
    ubo.projection = _projection;

    void* data;
    vkMapMemory(_device->getLogicalDevice(), _uniformBufferCamera->getBuffer()[currentFrame]->getMemory(), 0,
                sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(_device->getLogicalDevice(), _uniformBufferCamera->getBuffer()[currentFrame]->getMemory());
  }
  {
    std::array<UniformObjectPointLight, 3> lights{};
    lights[0].position = glm::vec3(1, 1, 1);
    lights[0].color = glm::vec3(1., 0.8f, 0.5f);
    lights[0].radius = 1.6f;
    lights[1].position = glm::vec3(0.15f, 0.97f, 0);
    lights[1].color = glm::vec3(5, 2, 0);
    lights[1].radius = 0.0f;
    lights[2].position = glm::vec3(0.15f, 0.97f, 0);
    lights[2].color = glm::vec3(2, 0.5f, 0);
    lights[2].radius = 0.8f;

    UniformObjectPointLights ubo{};
    ubo.number = 3;
    for (int i = 0; i < ubo.number; i++) {
      ubo.pLights[i] = lights[i];
    }
    void* data;
    vkMapMemory(_device->getLogicalDevice(), _uniformBufferPointLight->getBuffer()[currentFrame]->getMemory(), 0,
                sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(_device->getLogicalDevice(), _uniformBufferPointLight->getBuffer()[currentFrame]->getMemory());
  }

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