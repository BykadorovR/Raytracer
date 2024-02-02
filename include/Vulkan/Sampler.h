#pragma once
#include "State.h"

class Sampler {
 private:
  std::shared_ptr<State> _state;
  VkSampler _sampler;

 public:
  Sampler(VkSamplerAddressMode mode, int mipMapLevels, int anisotropicSamples, std::shared_ptr<State> state);
  VkSampler& getSampler();

  ~Sampler();
};