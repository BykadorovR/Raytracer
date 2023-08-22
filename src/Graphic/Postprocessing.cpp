#include "Postprocessing.h"

Postprocessing::Postprocessing(std::vector<std::shared_ptr<Texture>> src,
                               std::vector<std::shared_ptr<ImageView>> dst,
                               std::shared_ptr<State> state) {
  _state = state;

  auto shader = std::make_shared<Shader>(_state->getDevice());
  shader->add("../shaders/postprocess_compute.spv", VK_SHADER_STAGE_COMPUTE_BIT);

  auto textureLayout = std::make_shared<DescriptorSetLayout>(_state->getDevice());
  textureLayout->createPostprocessing();

  for (int i = 0; i < src.size(); i++) {
    for (int j = 0; j < dst.size(); j++) {
      _descriptorSet[std::pair(i, j)] = std::make_shared<DescriptorSet>(1, textureLayout, _state->getDescriptorPool(),
                                                                        _state->getDevice());
      _descriptorSet[std::pair(i, j)]->createPostprocessing(src[i]->getImageView(), dst[j]);
    }
  }

  _computePipeline = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _computePipeline->createParticleSystemCompute(shader->getShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT),
                                                {std::pair{std::string("texture"), textureLayout}});
}

void Postprocessing::drawCompute(int currentFrame, int swapchainIndex, std::shared_ptr<CommandBuffer> commandBuffer) {
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
                    _computePipeline->getPipeline());

  auto pipelineLayout = _computePipeline->getDescriptorSetLayout();
  auto computeLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                    [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                      return info.first == std::string("texture");
                                    });
  if (computeLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
                            _computePipeline->getPipelineLayout(), 0, 1,
                            &_descriptorSet[std::pair(currentFrame, swapchainIndex)]->getDescriptorSets()[0], 0,
                            nullptr);
  }

  auto [width, height] = _state->getSettings()->getResolution();
  vkCmdDispatch(commandBuffer->getCommandBuffer()[currentFrame], std::max(1, (int)std::ceil(width / 16.f)),
                std::max(1, (int)std::ceil(height / 16.f)), 1);
}