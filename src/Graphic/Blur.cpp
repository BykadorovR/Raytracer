#include "Blur.h"
#include <numbers>

std::vector<float> Blur::_generateWeights(int kernelSize, float variance) {
  float expectedValue = 0;
  std::vector<float> coeffs;
  for (int i = -kernelSize / 2; i <= kernelSize / 2; i++) {
    float value = (1 / (sqrt(variance) * sqrt(2 * std::numbers::pi))) *
                  std::exp(-pow(i - expectedValue, 2) / (2 * variance));
    coeffs.push_back(value);
  }

  return coeffs;
}

Blur::Blur(std::vector<std::shared_ptr<Texture>> src,
           std::vector<std::shared_ptr<Texture>> dst,
           std::shared_ptr<State> state) {
  _state = state;
  auto weights = _generateWeights(11, 1);

  auto shaderVertical = std::make_shared<Shader>(_state->getDevice());
  shaderVertical->add("../shaders/blurVertical_compute.spv", VK_SHADER_STAGE_COMPUTE_BIT);
  auto shaderHorizontal = std::make_shared<Shader>(_state->getDevice());
  shaderHorizontal->add("../shaders/blurHorizontal_compute.spv", VK_SHADER_STAGE_COMPUTE_BIT);

  auto textureLayout = std::make_shared<DescriptorSetLayout>(_state->getDevice());
  textureLayout->createPostprocessing();
  _descriptorSetHorizontal = std::make_shared<DescriptorSet>(
      _state->getSettings()->getMaxFramesInFlight(), textureLayout, _state->getDescriptorPool(), _state->getDevice());
  _descriptorSetHorizontal->createBlur(src, dst);

  _descriptorSetVertical = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(), textureLayout,
                                                           _state->getDescriptorPool(), _state->getDevice());
  _descriptorSetVertical->createBlur(dst, src);

  _computePipelineVertical = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _computePipelineVertical->createParticleSystemCompute(shaderVertical->getShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT),
                                                        {std::pair{std::string("texture"), textureLayout}}, {});
  _computePipelineHorizontal = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _computePipelineHorizontal->createParticleSystemCompute(
      shaderHorizontal->getShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT),
      {std::pair{std::string("texture"), textureLayout}}, {});
}

void Blur::drawCompute(int currentFrame, bool horizontal, std::shared_ptr<CommandBuffer> commandBuffer) {
  std::shared_ptr<Pipeline> pipeline = _computePipelineVertical;
  std::shared_ptr<DescriptorSet> descriptorSet = _descriptorSetVertical;
  float groupCountX = 1.f;
  float groupCountY = 64.f;
  if (horizontal) {
    pipeline = _computePipelineHorizontal;
    descriptorSet = _descriptorSetHorizontal;
    groupCountX = 64.f;
    groupCountY = 1.f;
  }
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
                    pipeline->getPipeline());

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

  auto [width, height] = _state->getSettings()->getResolution();
  vkCmdDispatch(commandBuffer->getCommandBuffer()[currentFrame], std::max(1, (int)std::ceil(width / groupCountX)),
                std::max(1, (int)std::ceil(height / groupCountY)), 1);
}