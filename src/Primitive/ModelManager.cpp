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
    _descriptorSetLayout.push_back({"camera", setLayout});
  }
  {
    auto setLayout = std::make_shared<DescriptorSetLayout>(device);
    setLayout->createGraphicModel();
    _descriptorSetLayout.push_back({"texture", setLayout});
  }
  {
    auto setLayout = std::make_shared<DescriptorSetLayout>(device);
    setLayout->createJoints();
    _descriptorSetLayout.push_back({"joint", setLayout});
  }
  {
    auto setLayout = std::make_shared<DescriptorSetLayout>(device);
    setLayout->createModelAuxilary();
    _descriptorSetLayout.push_back({"alphaMask", setLayout});
  }
  { _descriptorSetLayout.push_back({"light", _lightManager->getDSLLight()}); }

  { _descriptorSetLayout.push_back({"lightVP", _lightManager->getDSLViewProjection()}); }

  { _descriptorSetLayout.push_back({"shadowTexture", _lightManager->getDSLShadowTexture()}); }

  {
    auto shader = std::make_shared<Shader>(device);
    shader->add("../shaders/phong3D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/phong3D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[ModelRenderMode::FULL] = std::make_shared<Pipeline>(shader, device);
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["vertex"] = PushConstants::getPushConstant(sizeof(LightPush));
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
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["vertex"] = PushConstants::getPushConstant(0);
    _pipeline[ModelRenderMode::DEPTH]->createGraphic3DShadow(
        VK_CULL_MODE_NONE, {_descriptorSetLayout[0], _descriptorSetLayout[2]}, defaultPushConstants,
        Vertex3D::getBindingDescription(), Vertex3D::getAttributeDescriptions(), renderDepth);

    _pipelineCullOff[ModelRenderMode::DEPTH] = std::make_shared<Pipeline>(shader, device);
    _pipelineCullOff[ModelRenderMode::DEPTH]->createGraphic3DShadow(
        VK_CULL_MODE_NONE, {_descriptorSetLayout[0], _descriptorSetLayout[2]}, defaultPushConstants,
        Vertex3D::getBindingDescription(), Vertex3D::getAttributeDescriptions(), renderDepth);
  }
}

std::shared_ptr<ModelGLTF> Model3DManager::createModelGLTF(std::string path) {
  _modelsCreated++;
  return std::make_shared<ModelGLTF>(path, _descriptorSetLayout, _lightManager, _renderPass, _descriptorPool,
                                     _commandPool, _commandBuffer, _queue, _device, _settings);
}
void Model3DManager::registerModelGLTF(std::shared_ptr<Model> model) { _modelsGLTF.push_back(model); }

void Model3DManager::unregisterModelGLTF(std::shared_ptr<Model> model) {
  _modelsGLTF.erase(std::remove(_modelsGLTF.begin(), _modelsGLTF.end(), model), _modelsGLTF.end());
}

void Model3DManager::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

void Model3DManager::draw(int currentFrame, float frameTimer) {
  vkCmdBindPipeline(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline[ModelRenderMode::FULL]->getPipeline());

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

  if (_pipeline[ModelRenderMode::FULL]->getPushConstants().find("fragment") !=
      _pipeline[ModelRenderMode::FULL]->getPushConstants().end()) {
    LightPush pushConstants;
    pushConstants.cameraPosition = _camera->getEye();
    vkCmdPushConstants(_commandBuffer->getCommandBuffer()[currentFrame],
                       _pipeline[ModelRenderMode::FULL]->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(LightPush), &pushConstants);
  }

  auto pipelineLayout = _pipeline[ModelRenderMode::FULL]->getDescriptorSetLayout();
  auto lightVPLayout = std::find_if(
      pipelineLayout.begin(), pipelineLayout.end(),
      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "lightVP"; });
  if (lightVPLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline[ModelRenderMode::FULL]->getPipelineLayout(), 5, 1,
                            &_lightManager->getDSViewProjection()->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  auto shadowTextureLayout = std::find_if(
      pipelineLayout.begin(), pipelineLayout.end(),
      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "shadowTexture"; });
  if (shadowTextureLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline[ModelRenderMode::FULL]->getPipelineLayout(), 6, 1,
                            &_lightManager->getDSShadowTexture()[currentFrame]->getDescriptorSets()[currentFrame], 0,
                            nullptr);
  }

  auto lightLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                  [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                    return info.first == std::string("light");
                                  });
  if (lightLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline[ModelRenderMode::FULL]->getPipelineLayout(), 4, 1,
                            &_lightManager->getDSLight()->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  for (auto model : _modelsGLTF) {
    model->setCamera(_camera);
    model->draw(currentFrame, _pipeline[ModelRenderMode::FULL], _pipelineCullOff[ModelRenderMode::FULL], frameTimer);
  }
}

void Model3DManager::drawShadow(int currentFrame, LightType lightType, int lightIndex, float frameTimer, int face) {
  vkCmdBindPipeline(_commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline[ModelRenderMode::DEPTH]->getPipeline());

  auto [width, height] = _settings->getResolution();
  int resolution = std::max(width, height);
  VkViewport viewport{};
  viewport.x = 0.0f;
  if (lightType == LightType::DIRECTIONAL) {
    viewport.y = 0;
    viewport.width = std::get<0>(_settings->getResolution());
    viewport.height = std::get<1>(_settings->getResolution());
  }
  // in case of POINT light we have cubemap it should be equal sized, so width = height
  if (lightType == LightType::POINT) {
    viewport.y = 0;
    viewport.width = resolution;
    viewport.height = resolution;
  }
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(_commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(resolution, resolution);
  vkCmdSetScissor(_commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  for (auto model : _modelsGLTF) {
    if (model->isDebug()) continue;
    glm::mat4 view(1.f);
    glm::mat4 projection(1.f);
    int lightIndexTotal = lightIndex;
    if (lightType == LightType::DIRECTIONAL) {
      view = _lightManager->getDirectionalLights()[lightIndex]->getViewMatrix();
      projection = _lightManager->getDirectionalLights()[lightIndex]->getProjectionMatrix();
    }
    if (lightType == LightType::POINT) {
      lightIndexTotal += _settings->getMaxDirectionalLights();
      view = _lightManager->getPointLights()[lightIndex]->getViewMatrix(face);
      projection = _lightManager->getPointLights()[lightIndex]->getProjectionMatrix();
    }

    model->drawShadow(currentFrame, _pipeline[ModelRenderMode::DEPTH], _pipelineCullOff[ModelRenderMode::DEPTH],
                      lightIndexTotal, view, projection, frameTimer, face);
  }
}