#include "Sprite.h"

struct UniformObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 projection;
};

Sprite::Sprite(std::shared_ptr<Texture> texture,
               std::shared_ptr<Texture> normalMap,
               std::shared_ptr<Texture> shadowMap,
               std::vector<std::shared_ptr<DescriptorSetLayout>> descriptorSetLayout,
               std::shared_ptr<DescriptorPool> descriptorPool,
               std::shared_ptr<CommandPool> commandPool,
               std::shared_ptr<CommandBuffer> commandBuffer,
               std::shared_ptr<Queue> queue,
               std::shared_ptr<Device> device,
               std::shared_ptr<Settings> settings) {
  _commandBuffer = commandBuffer;
  _device = device;
  _settings = settings;
  _texture = texture;
  _normalMap = normalMap;

  _vertexBuffer = std::make_shared<VertexBuffer2D>(_vertices, commandPool, queue, device);
  _indexBuffer = std::make_shared<IndexBuffer>(_indices, commandPool, queue, device);
  _uniformBuffer[SpriteRenderMode::DEPTH] = std::make_shared<UniformBuffer>(
      settings->getMaxFramesInFlight(), sizeof(UniformObject), commandPool, queue, device);
  _uniformBuffer[SpriteRenderMode::FULL] = std::make_shared<UniformBuffer>(
      settings->getMaxFramesInFlight(), sizeof(UniformObject), commandPool, queue, device);
  {
    auto cameraSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), descriptorSetLayout[0],
                                                     descriptorPool, device);
    cameraSet->createCamera(_uniformBuffer[SpriteRenderMode::DEPTH]);
    _descriptorSetCamera[SpriteRenderMode::DEPTH] = cameraSet;
  }
  {
    auto cameraSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), descriptorSetLayout[0],
                                                     descriptorPool, device);
    cameraSet->createCamera(_uniformBuffer[SpriteRenderMode::FULL]);
    _descriptorSetCamera[SpriteRenderMode::FULL] = cameraSet;
  }
  {
    auto textureSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), descriptorSetLayout[1],
                                                      descriptorPool, device);
    textureSet->createGraphicModel(texture, normalMap, shadowMap);
    _descriptorSetTextures = textureSet;
  }
}

void Sprite::setModel(glm::mat4 model) { _model = model; }

void Sprite::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

void Sprite::setNormal(glm::vec3 normal) {
  for (auto& vertex : _vertices) {
    vertex.normal = normal;
  }
}

void Sprite::draw(int currentFrame, SpriteRenderMode mode, std::shared_ptr<Pipeline> pipeline) {
  UniformObject cameraUBO{};
  cameraUBO.model = _model;
  cameraUBO.view = _camera->getView();
  cameraUBO.projection = _camera->getProjection();

  void* data;
  vkMapMemory(_device->getLogicalDevice(), _uniformBuffer[mode]->getBuffer()[currentFrame]->getMemory(), 0,
              sizeof(cameraUBO), 0, &data);
  memcpy(data, &cameraUBO, sizeof(cameraUBO));
  vkUnmapMemory(_device->getLogicalDevice(), _uniformBuffer[mode]->getBuffer()[currentFrame]->getMemory());

  VkBuffer vertexBuffers[] = {_vertexBuffer->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(_commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(_commandBuffer->getCommandBuffer()[currentFrame], _indexBuffer->getBuffer()->getData(), 0,
                       VK_INDEX_TYPE_UINT32);

  if (pipeline->getDescriptorSetLayout().size() > 0) {
    vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->getPipelineLayout(), 0, 1,
                            &_descriptorSetCamera[mode]->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  if (pipeline->getDescriptorSetLayout().size() > 1) {
    vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->getPipelineLayout(), 1, 1,
                            &_descriptorSetTextures->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  vkCmdDrawIndexed(_commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_indices.size()), 1, 0, 0,
                   0);
}