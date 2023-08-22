#pragma once
#include "State.h"
#include "Texture.h"

class Postprocessing {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Pipeline> _computePipeline;
  std::map<std::pair<int, int>, std::shared_ptr<DescriptorSet>> _descriptorSet;

 public:
  Postprocessing(std::vector<std::shared_ptr<Texture>> src,
                 std::vector<std::shared_ptr<ImageView>> dst,
                 std::shared_ptr<State> state);
  void drawCompute(int currentFrame, int swapchainIndex, std::shared_ptr<CommandBuffer> commandBuffer);
};