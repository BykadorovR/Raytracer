#pragma once
#include "Utility/EngineState.h"

class Sampler {
 private:
  std::shared_ptr<EngineState> _engineState;
  VkSampler _sampler;

 public:
  Sampler(VkSamplerAddressMode mode,
          int mipMapLevels,
          int anisotropicSamples,
          VkFilter filter,
          std::shared_ptr<EngineState> engineState);
  VkSampler& getSampler();

  ~Sampler();
};