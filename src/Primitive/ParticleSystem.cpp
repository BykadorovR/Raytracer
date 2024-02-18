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
  _particles = particles;
  _state = state;
  _commandBufferTransfer = commandBufferTransfer;
  _texture = texture;

  _initializeCompute();
  _initializeGraphic();
}

void ParticleSystem::_initializeGraphic() {
  auto shader = std::make_shared<Shader>(_state->getDevice());
  shader->add("shaders/particles/particle_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader->add("shaders/particles/particle_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  auto cameraLayout = std::make_shared<DescriptorSetLayout>(_state->getDevice());
  cameraLayout->createUniformBuffer();

  _cameraUniformBuffer = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(),
                                                         sizeof(BufferMVP), _state);

  _descriptorSetCamera = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(), cameraLayout,
                                                         _state->getDescriptorPool(), _state->getDevice());
  _descriptorSetCamera->createUniformBuffer(_cameraUniformBuffer);

  auto textureLayout = std::make_shared<DescriptorSetLayout>(_state->getDevice());
  textureLayout->createTexture();

  // TODO: what's the point to use max frames in flight for static textures?
  _descriptorSetTexture = std::make_shared<DescriptorSet>(1, textureLayout, _state->getDescriptorPool(),
                                                          _state->getDevice());
  _descriptorSetTexture->createTexture({_texture});

  _graphicPipeline = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _graphicPipeline->createParticleSystemGraphic(
      std::vector{_state->getSettings()->getGraphicColorFormat(), _state->getSettings()->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
      {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
       shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
      {std::pair{std::string("camera"), cameraLayout}, std::pair{std::string("texture"), textureLayout}},
      std::map<std::string, VkPushConstantRange>{{std::string("vertex"), VertexConstants::getPushConstant()}},
      Particle::getBindingDescription(), Particle::getAttributeDescriptions());
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

  auto shader = std::make_shared<Shader>(_state->getDevice());
  shader->add("shaders/particles/particle_compute.spv", VK_SHADER_STAGE_COMPUTE_BIT);

  _computePipeline = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _computePipeline->createParticleSystemCompute(shader->getShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT),
                                                {{"computeSSBO", setLayoutSSBOCompute}}, {});
}

void ParticleSystem::setModel(glm::mat4 model) { _model = model; }

void ParticleSystem::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

void ParticleSystem::setPointScale(float pointScale) { _pointScale = pointScale; }

void ParticleSystem::updateTimer(float frameTimer) { _frameTimer = frameTimer; }

glm::mat4 ParticleSystem::getModel() { return _model; }

void ParticleSystem::drawCompute(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer) {
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

void ParticleSystem::drawGraphic(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer) {
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _graphicPipeline->getPipeline());
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = std::get<1>(_state->getSettings()->getResolution());
  viewport.width = std::get<0>(_state->getSettings()->getResolution());
  viewport.height = -std::get<1>(_state->getSettings()->getResolution());
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(std::get<0>(_state->getSettings()->getResolution()),
                              std::get<1>(_state->getSettings()->getResolution()));
  vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  if (_graphicPipeline->getPushConstants().find("vertex") != _graphicPipeline->getPushConstants().end()) {
    VertexConstants pushConstants;
    pushConstants.pointScale = _pointScale;
    vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], _graphicPipeline->getPipelineLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VertexConstants), &pushConstants);
  }

  BufferMVP cameraUBO{};
  cameraUBO.model = _model;
  cameraUBO.view = _camera->getView();
  cameraUBO.projection = _camera->getProjection();

  void* data;
  vkMapMemory(_state->getDevice()->getLogicalDevice(), _cameraUniformBuffer->getBuffer()[currentFrame]->getMemory(), 0,
              sizeof(cameraUBO), 0, &data);
  memcpy(data, &cameraUBO, sizeof(cameraUBO));
  vkUnmapMemory(_state->getDevice()->getLogicalDevice(), _cameraUniformBuffer->getBuffer()[currentFrame]->getMemory());

  VkBuffer vertexBuffers[] = {_particlesBuffer[currentFrame]->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  auto pipelineLayout = _graphicPipeline->getDescriptorSetLayout();
  auto cameraLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                   [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                     return info.first == std::string("camera");
                                   });
  if (cameraLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _graphicPipeline->getPipelineLayout(), 0, 1,
                            &_descriptorSetCamera->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  auto textureLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                    [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                      return info.first == std::string("texture");
                                    });
  if (textureLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _graphicPipeline->getPipelineLayout(), 1, 1, &_descriptorSetTexture->getDescriptorSets()[0],
                            0, nullptr);
  }

  vkCmdDraw(commandBuffer->getCommandBuffer()[currentFrame], _particles.size(), 1, 0, 0);
}