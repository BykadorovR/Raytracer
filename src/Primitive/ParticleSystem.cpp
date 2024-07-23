#include "ParticleSystem.h"

struct VertexConstants {
  float pointScale;  // nominator of gl_PointSize
  static VkPushConstantRange getPushConstant() {
    VkPushConstantRange pushConstant;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(VertexConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    return pushConstant;
  }
};

ParticleSystem::ParticleSystem(std::vector<Particle> particles,
                               std::shared_ptr<Texture> texture,
                               std::shared_ptr<CommandBuffer> commandBufferTransfer,
                               std::shared_ptr<State> state) {
  setName("Particle system");
  _particles = particles;
  _state = state;
  _commandBufferTransfer = commandBufferTransfer;
  _texture = texture;

  _initializeCompute();
  _initializeGraphic();
}

void ParticleSystem::_initializeGraphic() {
  auto shader = std::make_shared<Shader>(_state);
  shader->add("shaders/particles/particle_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader->add("shaders/particles/particle_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  _renderPass = std::make_shared<RenderPass>(_state->getSettings(), _state->getDevice());
  _renderPass->initializeGraphic();

  _cameraUniformBuffer = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(),
                                                         sizeof(BufferMVP), _state);

  auto descriptorSetLayoutGraphic = std::make_shared<DescriptorSetLayout>(_state->getDevice());
  std::vector<VkDescriptorSetLayoutBinding> layoutGraphic(2);
  layoutGraphic[0].binding = 0;
  layoutGraphic[0].descriptorCount = 1;
  layoutGraphic[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  layoutGraphic[0].pImmutableSamplers = nullptr;
  layoutGraphic[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  layoutGraphic[1].binding = 1;
  layoutGraphic[1].descriptorCount = 1;
  layoutGraphic[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutGraphic[1].pImmutableSamplers = nullptr;
  layoutGraphic[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  descriptorSetLayoutGraphic->createCustom(layoutGraphic);

  _descriptorSetGraphic = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                          descriptorSetLayoutGraphic, _state->getDescriptorPool(),
                                                          _state->getDevice());

  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoNormalsMesh;
    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
    std::vector<VkDescriptorBufferInfo> bufferInfoVertex(1);
    // write to binding = 0 for vertex shader
    bufferInfoVertex[0].buffer = _cameraUniformBuffer->getBuffer()[i]->getData();
    bufferInfoVertex[0].offset = 0;
    bufferInfoVertex[0].range = sizeof(BufferMVP);
    bufferInfoNormalsMesh[0] = bufferInfoVertex;
    // write for binding = 1 for geometry shader
    // write for binding = 1 for textures
    std::vector<VkDescriptorImageInfo> bufferInfoTexture(1);
    bufferInfoTexture[0].imageLayout = _texture->getImageView()->getImage()->getImageLayout();
    bufferInfoTexture[0].imageView = _texture->getImageView()->getImageView();
    bufferInfoTexture[0].sampler = _texture->getSampler()->getSampler();
    textureInfoColor[1] = bufferInfoTexture;

    _descriptorSetGraphic->createCustom(i, bufferInfoNormalsMesh, textureInfoColor);
  }

  _graphicPipeline = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _graphicPipeline->createParticleSystemGraphic(
      VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
      {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
       shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
      {std::pair{std::string("graphic"), descriptorSetLayoutGraphic}},
      std::map<std::string, VkPushConstantRange>{{std::string("vertex"), VertexConstants::getPushConstant()}},
      Particle::getBindingDescription(), Particle::getAttributeDescriptions(), _renderPass);
}

void ParticleSystem::_initializeCompute() {
  // TODO: change to VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
  _particlesBuffer.resize(_state->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    _particlesBuffer[i] = std::make_shared<Buffer>(
        _particles.size() * sizeof(Particle), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);
    _particlesBuffer[i]->map();
    memcpy((uint8_t*)(_particlesBuffer[i]->getMappedMemory()), _particles.data(), _particles.size() * sizeof(Particle));
    _particlesBuffer[i]->unmap();
  }

  _deltaUniformBuffer = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(float),
                                                        _state);

  auto setLayoutSSBOCompute = std::make_shared<DescriptorSetLayout>(_state->getDevice());
  setLayoutSSBOCompute->createParticleComputeBuffer();

  _descriptorSetCompute.resize(_state->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    // for 0 frame we have ssbo in + ssbo out, for 1 frame we have ssbo out + ssbo in
    _descriptorSetCompute[i] = std::make_shared<DescriptorSet>(1, setLayoutSSBOCompute, _state->getDescriptorPool(),
                                                               _state->getDevice());
    _descriptorSetCompute[i]->createParticleComputeBuffer(
        _deltaUniformBuffer, _particlesBuffer[abs(i - 1) % _state->getSettings()->getMaxFramesInFlight()],
        _particlesBuffer[i]);
  }

  auto shader = std::make_shared<Shader>(_state);
  shader->add("shaders/particles/particle_compute.spv", VK_SHADER_STAGE_COMPUTE_BIT);

  _computePipeline = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _computePipeline->createParticleSystemCompute(shader->getShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT),
                                                {{"computeSSBO", setLayoutSSBOCompute}}, {});
}

void ParticleSystem::setPointScale(float pointScale) { _pointScale = pointScale; }

void ParticleSystem::updateTimer(float frameTimer) { _frameTimer = frameTimer; }

void ParticleSystem::drawCompute(std::shared_ptr<CommandBuffer> commandBuffer) {
  int currentFrame = _state->getFrameInFlight();
  float timeDelta = _frameTimer * 2.f;
  void* data;
  vkMapMemory(_state->getDevice()->getLogicalDevice(), _deltaUniformBuffer->getBuffer()[currentFrame]->getMemory(), 0,
              sizeof(timeDelta), 0, &data);
  memcpy(data, &timeDelta, sizeof(timeDelta));
  vkUnmapMemory(_state->getDevice()->getLogicalDevice(), _deltaUniformBuffer->getBuffer()[currentFrame]->getMemory());

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
                            &_descriptorSetCompute[currentFrame]->getDescriptorSets()[0], 0, nullptr);
  }

  vkCmdDispatch(commandBuffer->getCommandBuffer()[currentFrame], std::max(1, (int)std::ceil(_particles.size() / 16.f)),
                1, 1);
}

void ParticleSystem::draw(std::tuple<int, int> resolution,
                          std::shared_ptr<Camera> camera,
                          std::shared_ptr<CommandBuffer> commandBuffer) {
  int currentFrame = _state->getFrameInFlight();
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _graphicPipeline->getPipeline());
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = std::get<1>(resolution);
  viewport.width = std::get<0>(resolution);
  viewport.height = -std::get<1>(resolution);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(std::get<0>(resolution), std::get<1>(resolution));
  vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  if (_graphicPipeline->getPushConstants().find("vertex") != _graphicPipeline->getPushConstants().end()) {
    VertexConstants pushConstants;
    pushConstants.pointScale = _pointScale;
    vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], _graphicPipeline->getPipelineLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VertexConstants), &pushConstants);
  }

  BufferMVP cameraUBO{};
  cameraUBO.model = _model;
  cameraUBO.view = camera->getView();
  cameraUBO.projection = camera->getProjection();

  void* data;
  vkMapMemory(_state->getDevice()->getLogicalDevice(), _cameraUniformBuffer->getBuffer()[currentFrame]->getMemory(), 0,
              sizeof(cameraUBO), 0, &data);
  memcpy(data, &cameraUBO, sizeof(cameraUBO));
  vkUnmapMemory(_state->getDevice()->getLogicalDevice(), _cameraUniformBuffer->getBuffer()[currentFrame]->getMemory());

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