#pragma once
#include "Utility/EngineState.h"
#include "Graphic/Texture.h"
#include "Vulkan/Descriptor.h"
#include "Vulkan/Pipeline.h"
#include "Primitive/Mesh.h"

class Blur {
 private:
  std::shared_ptr<EngineState> _engineState;
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
       std::shared_ptr<EngineState> engineState);
  void reset(std::vector<std::shared_ptr<Texture>> src, std::vector<std::shared_ptr<Texture>> dst);
  int getKernelSize();
  int getSigma();
  void setKernelSize(int kernelSize);
  void setSigma(int sigma);
  void drawCompute(int currentFrame, bool horizontal, std::shared_ptr<CommandBuffer> commandBuffer);
};

class BlurGraphic {
 private:
  std::shared_ptr<EngineState> _engineState;
  std::shared_ptr<Pipeline> _computePipelineVertical, _computePipelineHorizontal;
  std::shared_ptr<DescriptorSet> _descriptorSetVertical, _descriptorSetHorizontal;
  std::vector<std::shared_ptr<Buffer>> _blurWeightsSSBO;
  std::shared_ptr<DescriptorSetLayout> _layoutBlur;
  std::shared_ptr<RenderPass> _renderPass;
  std::shared_ptr<MeshStatic2D> _mesh;
  std::vector<float> _blurWeights;
  std::vector<bool> _changed;
  int _kernelSize = 3;
  float _sigma = _kernelSize / 3;
  std::tuple<int, int> _resolution;
  void _updateWeights();
  void _updateDescriptors(int currentFrame);
  void _setWeights(int currentFrame);
  void _initialize(std::vector<std::shared_ptr<Texture>> src, std::vector<std::shared_ptr<Texture>> dst);

 public:
  BlurGraphic(std::vector<std::shared_ptr<Texture>> src,
              std::vector<std::shared_ptr<Texture>> dst,
              std::shared_ptr<CommandBuffer> commandBufferTransfer,
              std::shared_ptr<EngineState> engineState);
  void reset(std::vector<std::shared_ptr<Texture>> src, std::vector<std::shared_ptr<Texture>> dst);
  int getKernelSize();
  int getSigma();
  void setKernelSize(int kernelSize);
  void setSigma(int sigma);
  void draw(bool horizontal, std::shared_ptr<CommandBuffer> commandBuffer);
};