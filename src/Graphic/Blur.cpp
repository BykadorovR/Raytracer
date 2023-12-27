#include "Blur.h"
#include <numbers>

void Blur::_updateWeights() {
  _blurWeights.clear();
  float expectedValue = 0;
  float sum = 0;
  for (int i = -_kernelSize / 2; i <= _kernelSize / 2; i++) {
    float value = std::exp(-pow(i, 2) / (2 * pow(_sigma, 2)));
    _blurWeights.push_back(value);
    sum += value;
  }

  for (auto& coeff : _blurWeights) coeff /= sum;
}

void Blur::_updateDescriptors(int currentFrame) {
  _blurWeightsSSBO[currentFrame] = std::make_shared<Buffer>(
      _blurWeights.size() * sizeof(float), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());
  _blurWeightsSSBO[currentFrame]->map();
  memcpy((uint8_t*)(_blurWeightsSSBO[currentFrame]->getMappedMemory()), _blurWeights.data(),
         _blurWeights.size() * sizeof(float));
  _blurWeightsSSBO[currentFrame]->unmap();
}

void Blur::_setWeights(int currentFrame) {
  _descriptorSetWeights->createShaderStorageBuffer(currentFrame, {0}, {_blurWeightsSSBO});
}

int Blur::getKernelSize() { return _kernelSize; }

int Blur::getSigma() { return _sigma; }

void Blur::setKernelSize(int kernelSize) {
  kernelSize = std::min(kernelSize, 65);
  _kernelSize = kernelSize;
  _updateWeights();
  for (int i = 0; i < _changed.size(); i++) _changed[i] = true;
}

void Blur::setSigma(int sigma) {
  _sigma = sigma;
  _updateWeights();
  for (int i = 0; i < _changed.size(); i++) _changed[i] = true;
}

Blur::Blur(std::vector<std::shared_ptr<Texture>> src,
           std::vector<std::shared_ptr<Texture>> dst,
           std::shared_ptr<State> state) {
  _state = state;

  auto shaderVertical = std::make_shared<Shader>(_state->getDevice());
  shaderVertical->add("shaders/blurVertical_compute.spv", VK_SHADER_STAGE_COMPUTE_BIT);
  auto shaderHorizontal = std::make_shared<Shader>(_state->getDevice());
  shaderHorizontal->add("shaders/blurHorizontal_compute.spv", VK_SHADER_STAGE_COMPUTE_BIT);

  auto textureLayout = std::make_shared<DescriptorSetLayout>(_state->getDevice());
  textureLayout->createPostprocessing();

  _blurWeightsSSBO.resize(_state->getSettings()->getMaxFramesInFlight());
  _changed.resize(_state->getSettings()->getMaxFramesInFlight());

  auto bufferLayout = std::make_shared<DescriptorSetLayout>(_state->getDevice());
  bufferLayout->createShaderStorageBuffer({0}, {VK_SHADER_STAGE_COMPUTE_BIT});
  _descriptorSetWeights = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(), bufferLayout,
                                                          _state->getDescriptorPool(), _state->getDevice());

  _descriptorSetHorizontal = std::make_shared<DescriptorSet>(
      _state->getSettings()->getMaxFramesInFlight(), textureLayout, _state->getDescriptorPool(), _state->getDevice());
  _descriptorSetHorizontal->createBlur(src, dst);

  _descriptorSetVertical = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(), textureLayout,
                                                           _state->getDescriptorPool(), _state->getDevice());
  _descriptorSetVertical->createBlur(dst, src);

  _computePipelineVertical = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _computePipelineVertical->createParticleSystemCompute(
      shaderVertical->getShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT),
      {std::pair{std::string("texture"), textureLayout}, std::pair{std::string("weights"), bufferLayout}}, {});
  _computePipelineHorizontal = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _computePipelineHorizontal->createParticleSystemCompute(
      shaderHorizontal->getShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT),
      {std::pair{std::string("texture"), textureLayout}, std::pair{std::string("weights"), bufferLayout}}, {});

  _updateWeights();
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    _updateDescriptors(i);
    _setWeights(i);
    _changed[i] = false;
  }
}

void Blur::drawCompute(int currentFrame, bool horizontal, std::shared_ptr<CommandBuffer> commandBuffer) {
  std::shared_ptr<Pipeline> pipeline = _computePipelineVertical;
  std::shared_ptr<DescriptorSet> descriptorSet = _descriptorSetVertical;
  float groupCountX = 1.f;
  float groupCountY = 128.f;
  if (horizontal) {
    pipeline = _computePipelineHorizontal;
    descriptorSet = _descriptorSetHorizontal;
    groupCountX = 128.f;
    groupCountY = 1.f;
  }
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
                    pipeline->getPipeline());

  if (_changed[currentFrame]) {
    _updateDescriptors(currentFrame);
    _setWeights(currentFrame);
    _changed[currentFrame] = false;
  }

  auto pipelineLayout = pipeline->getDescriptorSetLayout();
  auto computeLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                    [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                      return info.first == std::string("texture");
                                    });
  if (computeLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipeline->getPipelineLayout(), 0, 1, &descriptorSet->getDescriptorSets()[currentFrame], 0,
                            nullptr);
  }

  auto weightsLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                    [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                      return info.first == std::string("weights");
                                    });
  if (weightsLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipeline->getPipelineLayout(), 1, 1,
                            &_descriptorSetWeights->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  auto [width, height] = _state->getSettings()->getResolution();
  vkCmdDispatch(commandBuffer->getCommandBuffer()[currentFrame], std::max(1, (int)std::ceil(width / groupCountX)),
                std::max(1, (int)std::ceil(height / groupCountY)), 1);
}