#include "ModelManager.h"

Model3DManager::Model3DManager(std::shared_ptr<CommandPool> commandPool,
                               std::shared_ptr<CommandBuffer> commandBuffer,
                               std::shared_ptr<Queue> queue,
                               std::shared_ptr<RenderPass> render,
                               std::shared_ptr<Device> device,
                               std::shared_ptr<Settings> settings) {
  _commandPool = commandPool;
  _commandBuffer = commandBuffer;
  _queue = queue;
  _device = device;
  _settings = settings;

  _descriptorPool.push_back(std::make_shared<DescriptorPool>(_descriptorPoolSize, device));
  _descriptorSetLayout = std::make_shared<DescriptorSetLayout>(device);
  _descriptorSetLayout->createGraphic();
  auto shader = std::make_shared<Shader>(device);
  shader->add("../shaders/simple_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader->add("../shaders/simple_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  _pipeline = std::make_shared<Pipeline>(shader, _descriptorSetLayout, device);
  _pipeline->createGraphic(render);
}

std::shared_ptr<Model3D> Model3DManager::createModel(std::string path, std::shared_ptr<Texture> texture) {
  if ((_modelsCreated * _settings->getMaxFramesInFlight()) >= _descriptorPoolSize * _descriptorPool.size()) {
    _descriptorPool.push_back(std::make_shared<DescriptorPool>(_descriptorPoolSize, _device));
  }
  _modelsCreated++;
  return std::make_shared<Model3D>(path, texture, _descriptorSetLayout, _pipeline, _descriptorPool.back(), _commandPool,
                                   _commandBuffer, _queue, _device, _settings);
}

void Model3DManager::registerModel(std::shared_ptr<Model3D> model) { _models.push_back(model); }

void Model3DManager::unregisterModel(std::shared_ptr<Model3D> model) {
  _models.erase(std::remove(_models.begin(), _models.end(), model), _models.end());
}

void Model3DManager::draw(int currentFrame) {
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

  for (auto sprite : _models) {
    sprite->draw(currentFrame);
  }
}