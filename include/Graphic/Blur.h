#pragma once
#include "State.h"
#include "Texture.h"

class Blur {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Pipeline> _computePipeline;
  std::shared_ptr<DescriptorSet> _descriptorSet;

 public:
  Blur(std::vector<std::shared_ptr<Texture>> src,
       std::vector<std::shared_ptr<Texture>> dst,
       std::shared_ptr<State> state);

  void drawCompute(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer);
};