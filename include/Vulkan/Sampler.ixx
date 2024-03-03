module;
export module Sampler;
import "vulkan/vulkan.hpp";
import State;

export namespace VulkanEngine {
class Sampler {
 private:
  std::shared_ptr<State> _state;
  VkSampler _sampler;

 public:
  Sampler(VkSamplerAddressMode mode, int mipMapLevels, int anisotropicSamples, std::shared_ptr<State> state);
  VkSampler& getSampler();

  ~Sampler();
};
}  // namespace VulkanEngine