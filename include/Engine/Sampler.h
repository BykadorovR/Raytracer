#pragma once
#include "Device.h"

class Sampler {
 private:
  std::shared_ptr<Device> _device;
  VkSampler _sampler;

 public:
  Sampler(VkSamplerAddressMode mode, std::shared_ptr<Device> device);
  VkSampler& getSampler();

  ~Sampler();
};