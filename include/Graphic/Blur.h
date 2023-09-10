#pragma once
#include "State.h"
#include "Texture.h"

class Blur {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Pipeline> _computePipelineVertical, _computePipelineHorizontal;
  std::shared_ptr<DescriptorSet> _descriptorSetVertical, _descriptorSetHorizontal;

  std::vector<float> _generateWeights(int kernelSize, float variance);

 public:
  Blur(std::vector<std::shared_ptr<Texture>> src,
       std::vector<std::shared_ptr<Texture>> dst,
       std::shared_ptr<State> state);

  void drawCompute(int currentFrame, bool horizontal, std::shared_ptr<CommandBuffer> commandBuffer);
};