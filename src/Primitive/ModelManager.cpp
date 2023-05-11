#include "ModelManager.h"

Model3DManager::Model3DManager(std::shared_ptr<LightManager> lightManager,
                               std::shared_ptr<CommandPool> commandPool,
                               std::shared_ptr<CommandBuffer> commandBuffer,
                               std::shared_ptr<Queue> queue,
                               std::shared_ptr<DescriptorPool> descriptorPool,
                               std::shared_ptr<RenderPass> render,
                               std::shared_ptr<RenderPass> renderDepth,
                               std::shared_ptr<Device> device,
                               std::shared_ptr<Settings> settings) {
  _commandPool = commandPool;
  _lightManager = lightManager;
  _commandBuffer = commandBuffer;
  _queue = queue;
  _device = device;
  _settings = settings;
  _renderPass = render;
  _descriptorPool = descriptorPool;
  {
    auto setLayout = std::make_shared<DescriptorSetLayout>(device);
    setLayout->createCamera();
    _descriptorSetLayout.push_back(setLayout);
  }
  {
    auto setLayout = std::make_shared<DescriptorSetLayout>(device);
    setLayout->createGraphicModel();
    _descriptorSetLayout.push_back(setLayout);
  }
  {
    auto setLayout = std::make_shared<DescriptorSetLayout>(device);
    setLayout->createJoints();
    _descriptorSetLayout.push_back(setLayout);
  }
  {
    auto setLayout = std::make_shared<DescriptorSetLayout>(device);
    setLayout->createModelAuxilary();
    _descriptorSetLayout.push_back(setLayout);
  }
  { _descriptorSetLayout.push_back(_lightManager->getDescriptorSetLayout()); }

  {
    _shadowDescriptorSetLayout = std::make_shared<DescriptorSetLayout>(device);
    _shadowDescriptorSetLayout->createShadow();
    _descriptorSetLayout.push_back(_shadowDescriptorSetLayout);
  }

  {
    auto shader = std::make_shared<Shader>(device);
    shader->add("../shaders/phong3D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/phong3D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[ModelRenderMode::FULL] = std::make_shared<Pipeline>(shader, device);
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["vertex"] = PushConstants::getPushConstant();
    defaultPushConstants["fragment"] = LightPush::getPushConstant();

    _pipeline[ModelRenderMode::FULL]->createGraphic3D(VK_CULL_MODE_BACK_BIT, _descriptorSetLayout, defaultPushConstants,
                                                      Vertex3D::getBindingDescription(),
                                                      Vertex3D::getAttributeDescriptions(), render);

    _pipelineCullOff[ModelRenderMode::FULL] = std::make_shared<Pipeline>(shader, device);
    _pipelineCullOff[ModelRenderMode::FULL]->createGraphic3D(VK_CULL_MODE_NONE, _descriptorSetLayout,
                                                             defaultPushConstants, Vertex3D::getBindingDescription(),
                                                             Vertex3D::getAttributeDescriptions(), render);
  }
  {
    auto shader = std::make_shared<Shader>(device);
    shader->add("../shaders/depth3D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/depth3D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[ModelRenderMode::DEPTH] = std::make_shared<Pipeline>(shader, device);
    _pipeline[ModelRenderMode::DEPTH]->createGraphic3D(VK_CULL_MODE_BACK_BIT, {_descriptorSetLayout[0]}, {},
                                                       Vertex3D::getBindingDescription(),
                                                       Vertex3D::getAttributeDescriptions(), renderDepth);

    _pipelineCullOff[ModelRenderMode::DEPTH] = std::make_shared<Pipeline>(shader, device);
    _pipelineCullOff[ModelRenderMode::DEPTH]->createGraphic3D(VK_CULL_MODE_NONE, {_descriptorSetLayout[0]}, {},
                                                              Vertex3D::getBindingDescription(),
                                                              Vertex3D::getAttributeDescriptions(), renderDepth);
  }

  {
    _uniformShadow = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(glm::mat4), commandPool,
                                                     queue, device);
    _shadowDescriptorSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), _shadowDescriptorSetLayout,
                                                           descriptorPool, device);
    _shadowDescriptorSet->createShadow(_uniformShadow);
  }

  _cameraOrtho = std::make_shared<CameraOrtho>();
}

std::shared_ptr<ModelGLTF> Model3DManager::createModelGLTF(std::string path, std::shared_ptr<Texture> shadowMap) {
  _modelsCreated++;
  return std::make_shared<ModelGLTF>(path, shadowMap, _descriptorSetLayout, _lightManager, _renderPass, _descriptorPool,
                                     _commandPool, _commandBuffer, _queue, _device, _settings);
}
void Model3DManager::registerModelGLTF(std::shared_ptr<Model> model) { _modelsGLTF.push_back(model); }

void Model3DManager::unregisterModelGLTF(std::shared_ptr<Model> model) {
  _modelsGLTF.erase(std::remove(_modelsGLTF.begin(), _modelsGLTF.end(), model), _modelsGLTF.end());
}

void Model3DManager::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

void Model3DManager::draw(int currentFrame, ModelRenderMode mode, float frameTimer) {
  vkCmdBindPipeline(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline[mode]->getPipeline());

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

  if (_pipeline[mode]->getPushConstants().find("fragment") != _pipeline[mode]->getPushConstants().end()) {
    LightPush pushConstants;
    pushConstants.cameraPosition = _camera->getEye();
    vkCmdPushConstants(_commandBuffer->getCommandBuffer()[currentFrame], _pipeline[mode]->getPipelineLayout(),
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LightPush), &pushConstants);
  }

  if (mode == ModelRenderMode::FULL) {
    auto position = _lightManager->getPointLights()[0]->getPosition();
    _cameraOrtho->setViewParameters(position, -position, glm::vec3(0.f, 1.f, 0.f));
    //_cameraOrtho->setViewParameters(_camera->getEye(), _camera->getDirection(), _camera->getUp());
    _cameraOrtho->setProjectionParameters({-10.f, 10.f, -10.f, 10.f}, 0.1f, 20.f);

    glm::mat4 shadowCamera = _cameraOrtho->getProjection() * _cameraOrtho->getView();

    void* data;
    vkMapMemory(_device->getLogicalDevice(), _uniformShadow->getBuffer()[currentFrame]->getMemory(), 0,
                sizeof(glm::mat4), 0, &data);
    memcpy(data, &shadowCamera, sizeof(glm::mat4));
    vkUnmapMemory(_device->getLogicalDevice(), _uniformShadow->getBuffer()[currentFrame]->getMemory());

    vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline[mode]->getPipelineLayout(), 5, 1,
                            &_shadowDescriptorSet->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  if (_pipeline[mode]->getDescriptorSetLayout().size() > 4) {
    vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline[mode]->getPipelineLayout(), 4, 1,
                            &_lightManager->getDescriptorSet()->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  for (auto model : _modelsGLTF) {
    if (mode == ModelRenderMode::DEPTH) {
      auto position = _lightManager->getPointLights()[0]->getPosition();
      _cameraOrtho->setViewParameters(position, -position, glm::vec3(0.f, 1.f, 0.f));
      //_cameraOrtho->setViewParameters(_camera->getEye(), _camera->getDirection(), _camera->getUp());
      _cameraOrtho->setProjectionParameters({-10.f, 10.f, -10.f, 10.f}, 0.1f, 20.f);
      model->setCamera(_cameraOrtho);
    }
    if (mode == ModelRenderMode::FULL) {
      model->setCamera(_camera);
    }
    model->draw(currentFrame, mode, _pipeline[mode], _pipelineCullOff[mode], frameTimer);
  }
}