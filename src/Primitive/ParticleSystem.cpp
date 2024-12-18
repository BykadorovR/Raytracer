#include "Primitive/ParticleSystem.h"

struct VertexPush {
  float pointScale;  // nominator of gl_PointSize
};

ParticleSystem::ParticleSystem(std::vector<Particle> particles,
                               std::shared_ptr<Texture> texture,
                               std::shared_ptr<CommandBuffer> commandBufferTransfer,
                               std::shared_ptr<GameState> gameState,
                               std::shared_ptr<EngineState> engineState) {
  setName("Particle system");
  _particles = particles;
  _engineState = engineState;
  _gameState = gameState;
  _commandBufferTransfer = commandBufferTransfer;
  _texture = texture;

  _initializeCompute();
  _initializeGraphic();
}

void ParticleSystem::_initializeGraphic() {
  auto shader = std::make_shared<Shader>(_engineState);
  shader->add("shaders/particles/particle_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader->add("shaders/particles/particle_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  _renderPass = _engineState->getRenderPassManager()->getRenderPass(RenderPassScenario::GRAPHIC);

  _cameraUniformBuffer.resize(_engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++)
    _cameraUniformBuffer[i] = std::make_shared<Buffer>(
        sizeof(BufferMVP), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);

  auto descriptorSetLayoutGraphic = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
  std::vector<VkDescriptorSetLayoutBinding> layoutGraphic{{.binding = 0,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                                           .pImmutableSamplers = nullptr},
                                                          {.binding = 1,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                           .pImmutableSamplers = nullptr}};

  descriptorSetLayoutGraphic->createCustom(layoutGraphic);

  _descriptorSetGraphic = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                          descriptorSetLayoutGraphic, _engineState);
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoNormalsMesh = {
        {0,
         {{.buffer = _cameraUniformBuffer[i]->getData(), .offset = 0, .range = _cameraUniformBuffer[i]->getSize()}}}};
    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor = {
        {1,
         {{.sampler = _texture->getSampler()->getSampler(),
           .imageView = _texture->getImageView()->getImageView(),
           .imageLayout = _texture->getImageView()->getImage()->getImageLayout()}}}};
    _descriptorSetGraphic->createCustom(i, bufferInfoNormalsMesh, textureInfoColor);
  }

  _graphicPipeline = std::make_shared<PipelineGraphic>(_engineState->getDevice());
  _graphicPipeline->setCullMode(VK_CULL_MODE_BACK_BIT);
  _graphicPipeline->setDepthTest(true);
  _graphicPipeline->setTopology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
  _graphicPipeline->createCustom(
      {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
       shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
      {std::pair{std::string("graphic"), descriptorSetLayoutGraphic}},
      std::map<std::string, VkPushConstantRange>{
          {std::string("vertex"),
           VkPushConstantRange{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = sizeof(VertexPush)}}},
      Particle::getBindingDescription(), Particle::getAttributeDescriptions(), _renderPass);
}

void ParticleSystem::_initializeCompute() {
  // TODO: change to VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
  _particlesBuffer.resize(_engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _particlesBuffer[i] = std::make_shared<Buffer>(
        _particles.size() * sizeof(Particle), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);
    _particlesBuffer[i]->setData(_particles.data());
  }

  _deltaUniformBuffer.resize(_engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++)
    _deltaUniformBuffer[i] = std::make_shared<Buffer>(
        sizeof(float), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);

  auto setLayoutSSBOCompute = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
  std::vector<VkDescriptorSetLayoutBinding> layoutBinding{{.binding = 0,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                           .pImmutableSamplers = nullptr},
                                                          {.binding = 1,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                           .pImmutableSamplers = nullptr},
                                                          {.binding = 2,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                           .pImmutableSamplers = nullptr}};
  setLayoutSSBOCompute->createCustom(layoutBinding);

  _descriptorSetCompute = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                          setLayoutSSBOCompute, _engineState);
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfo = {
        {0, {{.buffer = _deltaUniformBuffer[i]->getData(), .offset = 0, .range = _deltaUniformBuffer[i]->getSize()}}},
        {1,
         {{.buffer = _particlesBuffer[abs(i - 1) % _engineState->getSettings()->getMaxFramesInFlight()]->getData(),
           .offset = 0,
           .range = _particlesBuffer[abs(i - 1) % _engineState->getSettings()->getMaxFramesInFlight()]->getSize()}}},
        {2, {{.buffer = _particlesBuffer[i]->getData(), .offset = 0, .range = _particlesBuffer[i]->getSize()}}}};
    // for 0 frame we have ssbo in + ssbo out, for 1 frame we have ssbo out + ssbo in
    _descriptorSetCompute->createCustom(i, bufferInfo, {});
  }

  auto shader = std::make_shared<Shader>(_engineState);
  shader->add("shaders/particles/particle_compute.spv", VK_SHADER_STAGE_COMPUTE_BIT);

  _computePipeline = std::make_shared<PipelineCompute>(_engineState->getDevice());
  _computePipeline->createCustom(shader->getShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT),
                                 {{"computeSSBO", setLayoutSSBOCompute}}, {});
}

void ParticleSystem::setPointScale(float pointScale) { _pointScale = pointScale; }

void ParticleSystem::updateTimer(float frameTimer) { _frameTimer = frameTimer; }

void ParticleSystem::drawCompute(std::shared_ptr<CommandBuffer> commandBuffer) {
  int currentFrame = _engineState->getFrameInFlight();
  float timeDelta = _frameTimer * 2.f;
  _deltaUniformBuffer[currentFrame]->setData(&timeDelta);

  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
                    _computePipeline->getPipeline());

  auto pipelineLayout = _computePipeline->getDescriptorSetLayout();
  auto computeLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                    [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                      return info.first == std::string("computeSSBO");
                                    });
  if (computeLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
                            _computePipeline->getPipelineLayout(), 0, 1,
                            &_descriptorSetCompute->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  vkCmdDispatch(commandBuffer->getCommandBuffer()[currentFrame], std::max(1, (int)std::ceil(_particles.size() / 16.f)),
                1, 1);
}

void ParticleSystem::draw(std::shared_ptr<CommandBuffer> commandBuffer) {
  int currentFrame = _engineState->getFrameInFlight();
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _graphicPipeline->getPipeline());
  auto resolution = _engineState->getSettings()->getResolution();
  VkViewport viewport{.x = 0.0f,
                      .y = static_cast<float>(std::get<1>(resolution)),
                      .width = static_cast<float>(std::get<0>(resolution)),
                      .height = static_cast<float>(-std::get<1>(resolution)),
                      .minDepth = 0.f,
                      .maxDepth = 1.f};

  vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{.offset = {0, 0}, .extent = VkExtent2D(std::get<0>(resolution), std::get<1>(resolution))};
  vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  if (_graphicPipeline->getPushConstants().find("vertex") != _graphicPipeline->getPushConstants().end()) {
    VertexPush pushConstants{.pointScale = _pointScale};
    auto info = _graphicPipeline->getPushConstants()["vertex"];
    vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], _graphicPipeline->getPipelineLayout(),
                       info.stageFlags, info.offset, info.size, &pushConstants);
  }

  BufferMVP cameraUBO{.model = getModel(),
                      .view = _gameState->getCameraManager()->getCurrentCamera()->getView(),
                      .projection = _gameState->getCameraManager()->getCurrentCamera()->getProjection()};
  _cameraUniformBuffer[currentFrame]->setData(&cameraUBO);

  VkBuffer vertexBuffers[] = {_particlesBuffer[currentFrame]->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  auto pipelineLayout = _graphicPipeline->getDescriptorSetLayout();
  auto graphicLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                    [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                      return info.first == std::string("graphic");
                                    });
  if (graphicLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _graphicPipeline->getPipelineLayout(), 0, 1,
                            &_descriptorSetGraphic->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  vkCmdDraw(commandBuffer->getCommandBuffer()[currentFrame], _particles.size(), 1, 0, 0);
}