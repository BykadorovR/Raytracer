#include "Sampler.h"

Sampler::Sampler(VkSamplerAddressMode mode, int mipMapLevels, int anisotropicSamples, std::shared_ptr<Device> device) {
  _device = device;
  // sampler
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = mode;
  samplerInfo.addressModeV = mode;
  samplerInfo.addressModeW = mode;
  samplerInfo.anisotropyEnable = anisotropicSamples > 0 ? VK_TRUE : VK_FALSE;
  samplerInfo.maxAnisotropy = std::min((int)device->getDeviceLimits().maxSamplerAnisotropy, anisotropicSamples);
  samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = static_cast<float>(mipMapLevels);

  if (vkCreateSampler(device->getLogicalDevice(), &samplerInfo, nullptr, &_sampler) != VK_SUCCESS) {
    throw std::runtime_error("failed to create texture sampler!");
  }
}

VkSampler& Sampler::getSampler() { return _sampler; }

Sampler::~Sampler() { vkDestroySampler(_device->getLogicalDevice(), _sampler, nullptr); }