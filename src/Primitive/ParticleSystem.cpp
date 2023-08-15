#include "ParticleSystem.h"
#include <random>

ParticleSystem::ParticleSystem(int particlesNumber,
                               std::shared_ptr<CommandBuffer> commandBufferTransfer,
                               std::shared_ptr<State> state) {
  _particlesNumber = particlesNumber;
  _state = state;
  _commandBufferTransfer = commandBufferTransfer;

  _initializeCompute();
  _initializeGraphic();
}

void ParticleSystem::_initializeGraphic() {
  auto shader = std::make_shared<Shader>(_state->getDevice());
  shader->add("../shaders/particle_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader->add("../shaders/particle_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  _graphicPipeline = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _graphicPipeline->createParticleSystemGraphic(VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
                                                {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                 shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                {}, {}, Particle::getBindingDescription(),
                                                Particle::getAttributeDescriptions());
}

void ParticleSystem::_initializeCompute() {
  std::default_random_engine rndEngine((unsigned)time(nullptr));
  std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

  // Initial particle positions on a circle
  std::vector<Particle> particles(_particlesNumber);
  for (auto& particle : particles) {
    float r = 0.25f * sqrt(rndDist(rndEngine));
    float theta = rndDist(rndEngine) * 2 * 3.14159265358979323846;
    float x = r * cos(theta) * 800 / 600;
    float y = r * sin(theta);
    particle.position = glm::vec2(x, y);
    particle.velocity = glm::normalize(glm::vec2(x, y));
    particle.color = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
  }

  // TODO: change to VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
  _particlesBuffer.resize(_state->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    _particlesBuffer[i] = std::make_shared<Buffer>(
        _particlesNumber * sizeof(Particle), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());
    _particlesBuffer[i]->map();
    memcpy((uint8_t*)(_particlesBuffer[i]->getMappedMemory()), particles.data(), _particlesNumber * sizeof(Particle));
    _particlesBuffer[i]->unmap();
  }

  _deltaUniformBuffer = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(float),
                                                        _state->getDevice());

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
  shader->add("../shaders/particle_compute.spv", VK_SHADER_STAGE_COMPUTE_BIT);

  _computePipeline = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _computePipeline->createParticleSystemCompute(shader->getShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT),
                                                {{"computeSSBO", setLayoutSSBOCompute}});
}

void ParticleSystem::setModel(glm::mat4 model) { _model = model; }

void ParticleSystem::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

void ParticleSystem::updateTimer(float frameTimer) { _frameTimer = frameTimer; }

void ParticleSystem::drawCompute(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer) {
  // TODO: add image barrier
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

  // any read from SSBO should wait for write to SSBO
  VkMemoryBarrier memoryBarrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT};
  vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[currentFrame],
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // srcStageMask
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // dstStageMask
                       0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

  vkCmdDispatch(commandBuffer->getCommandBuffer()[currentFrame], _particlesNumber / 256, 1, 1);
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

  VkBuffer vertexBuffers[] = {_particlesBuffer[currentFrame]->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdDraw(commandBuffer->getCommandBuffer()[currentFrame], _particlesNumber, 1, 0, 0);
}