#include "Vulkan/Sampler.h"

Sampler::Sampler(VkSamplerAddressMode mode,
                 int mipMapLevels,
                 int anisotropicSamples,
                 VkFilter filter,
                 std::shared_ptr<EngineState> engineState) {
  _engineState = engineState;
  // sampler
  VkSamplerCreateInfo samplerInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = filter,
      .minFilter = filter,
      .addressModeU = mode,
      .addressModeV = mode,
      .addressModeW = mode,
      .mipLodBias = 0.0f,
      .anisotropyEnable = anisotropicSamples > 0 ? VK_TRUE : VK_FALSE,
      .maxAnisotropy = std::min(_engineState->getDevice()->getDeviceLimits().maxSamplerAnisotropy,
                                static_cast<float>(anisotropicSamples)),
      .compareEnable = VK_FALSE,
      .minLod = 0.0f,
      .maxLod = static_cast<float>(mipMapLevels),
      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
      .unnormalizedCoordinates = VK_FALSE};
  if (mipMapLevels > 1) samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

  if (vkCreateSampler(_engineState->getDevice()->getLogicalDevice(), &samplerInfo, nullptr, &_sampler) != VK_SUCCESS) {
    throw std::runtime_error("failed to create texture sampler!");
  }
}

VkSampler& Sampler::getSampler() { return _sampler; }

Sampler::~Sampler() { vkDestroySampler(_engineState->getDevice()->getLogicalDevice(), _sampler, nullptr); }