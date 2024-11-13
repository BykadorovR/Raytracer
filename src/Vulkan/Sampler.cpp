#include "Vulkan/Sampler.h"

Sampler::Sampler(VkSamplerAddressMode mode,
                 int mipMapLevels,
                 int anisotropicSamples,
                 VkFilter filter,
                 std::shared_ptr<EngineState> engineState) {
  _engineState = engineState;
  // sampler
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = filter;
  samplerInfo.minFilter = filter;
  samplerInfo.addressModeU = mode;
  samplerInfo.addressModeV = mode;
  samplerInfo.addressModeW = mode;
  samplerInfo.anisotropyEnable = anisotropicSamples > 0 ? VK_TRUE : VK_FALSE;
  samplerInfo.maxAnisotropy = std::min((int)_engineState->getDevice()->getDeviceLimits().maxSamplerAnisotropy,
                                       anisotropicSamples);
  samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  if (mipMapLevels > 1) samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = static_cast<float>(mipMapLevels);

  if (vkCreateSampler(_engineState->getDevice()->getLogicalDevice(), &samplerInfo, nullptr, &_sampler) != VK_SUCCESS) {
    throw std::runtime_error("failed to create texture sampler!");
  }
}

VkSampler& Sampler::getSampler() { return _sampler; }

Sampler::~Sampler() { vkDestroySampler(_engineState->getDevice()->getLogicalDevice(), _sampler, nullptr); }