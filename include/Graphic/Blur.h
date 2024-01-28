#pragma once
#include "State.h"
#include "Texture.h"
#include "Descriptor.h"
#include "Pipeline.h"

class Blur {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Pipeline> _computePipelineVertical, _computePipelineHorizontal;
  std::shared_ptr<DescriptorSet> _descriptorSetVertical, _descriptorSetHorizontal, _descriptorSetWeights;
  std::vector<std::shared_ptr<Buffer>> _blurWeightsSSBO;
  std::shared_ptr<DescriptorSetLayout> _textureLayout;
  std::vector<float> _blurWeights;
  std::vector<bool> _changed;
  int _kernelSize = 15, _sigma = 3;
  void _updateWeights();
  void _updateDescriptors(int currentFrame);
  void _setWeights(int currentFrame);
  void _initialize(std::vector<std::shared_ptr<Texture>> src, std::vector<std::shared_ptr<Texture>> dst);

 public:
  Blur(std::vector<std::shared_ptr<Texture>> src,
       std::vector<std::shared_ptr<Texture>> dst,
       std::shared_ptr<State> state);
  void reset(std::vector<std::shared_ptr<Texture>> src, std::vector<std::shared_ptr<Texture>> dst);
  int getKernelSize();
  int getSigma();
  void setKernelSize(int kernelSize);
  void setSigma(int sigma);
  void drawCompute(int currentFrame, bool horizontal, std::shared_ptr<CommandBuffer> commandBuffer);
};