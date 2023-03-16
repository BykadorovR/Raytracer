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

  auto shader3D = std::make_shared<Shader>(device);
  shader3D->add("../shaders/simple3D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader3D->add("../shaders/simple3D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

  auto shaderGLTF = std::make_shared<Shader>(device);
  shaderGLTF->add("../shaders/GLTF3D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shaderGLTF->add("../shaders/GLTF3D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

  _descriptorPool.push_back(std::make_shared<DescriptorPool>(_descriptorPoolSize, device));
  _descriptorSetLayout = std::make_shared<DescriptorSetLayout>(device);
  _descriptorSetLayout->createGraphic();
  _descriptorSetLayoutJoints = std::make_shared<DescriptorSetLayout>(_device);
  _descriptorSetLayoutJoints->createJoints();
  _pipeline = std::make_shared<Pipeline>(shader3D, std::vector{_descriptorSetLayout}, device);
  _pipeline->createGraphic3D(Vertex3D::getBindingDescription(), Vertex3D::getAttributeDescriptions(), render);

  _pipelineGLTF = std::make_shared<Pipeline>(shaderGLTF, std::vector{_descriptorSetLayout, _descriptorSetLayoutJoints},
                                             device);
  _pipelineGLTF->createGraphic3D(Vertex3D::getBindingDescription(), Vertex3D::getAttributeDescriptions(), render);
}

std::shared_ptr<ModelGLTF> Model3DManager::createModelGLTF(std::string path) {
  if ((_modelsCreated * _settings->getMaxFramesInFlight()) >= _descriptorPoolSize * _descriptorPool.size()) {
    _descriptorPool.push_back(std::make_shared<DescriptorPool>(_descriptorPoolSize, _device));
  }
  _modelsCreated++;
  return std::make_shared<ModelGLTF>(path, _descriptorSetLayout, _descriptorSetLayoutJoints, _pipelineGLTF,
                                     _descriptorPool.back(), _commandPool, _commandBuffer, _queue, _device, _settings);
}

std::shared_ptr<ModelOBJ> Model3DManager::createModel(std::string path) {
  if ((_modelsCreated * _settings->getMaxFramesInFlight()) >= _descriptorPoolSize * _descriptorPool.size()) {
    _descriptorPool.push_back(std::make_shared<DescriptorPool>(_descriptorPoolSize, _device));
  }
  _modelsCreated++;

  std::string texturePath = path;
  size_t lastPointPos = texturePath.find_last_of('.');
  if (lastPointPos != std::string::npos) {
    texturePath = texturePath.substr(0, lastPointPos + 1) + "png";
  }
  auto texture = std::make_shared<Texture>(texturePath, _commandPool, _queue, _device);
  return std::make_shared<ModelOBJ>(path, texture, _descriptorSetLayout, _pipeline, _descriptorPool.back(),
                                    _commandPool, _commandBuffer, _queue, _device, _settings);
}

void Model3DManager::registerModel(std::shared_ptr<Model> model) { _models.push_back(model); }

void Model3DManager::registerModelGLTF(std::shared_ptr<Model> model) { _modelsGLTF.push_back(model); }

void Model3DManager::unregisterModel(std::shared_ptr<Model> model) {
  _models.erase(std::remove(_models.begin(), _models.end(), model), _models.end());
}

void Model3DManager::draw(float frameTimer, int currentFrame) {
  vkCmdBindPipeline(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline->getPipeline());

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = std::get<1>(_settings->getResolution());
  viewport.width = std::get<0>(_settings->getResolution());
  viewport.height = -std::get<1>(_settings->getResolution());
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(_commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(std::get<0>(_settings->getResolution()), std::get<1>(_settings->getResolution()));
  vkCmdSetScissor(_commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  for (auto model : _models) {
    model->draw(frameTimer, currentFrame);
  }
}

void Model3DManager::drawGLTF(float frameTimer, int currentFrame) {
  vkCmdBindPipeline(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipelineGLTF->getPipeline());

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = std::get<1>(_settings->getResolution());
  viewport.width = std::get<0>(_settings->getResolution());
  viewport.height = -std::get<1>(_settings->getResolution());
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(_commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(std::get<0>(_settings->getResolution()), std::get<1>(_settings->getResolution()));
  vkCmdSetScissor(_commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  for (auto model : _modelsGLTF) {
    model->draw(frameTimer, currentFrame);
  }
}