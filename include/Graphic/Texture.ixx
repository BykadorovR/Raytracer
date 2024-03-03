module;
export module Texture;
import "stb_image.h";
import "vulkan/vulkan.hpp";
import Image;
import Sampler;
import State;
import Buffer;
import Command;

export namespace VulkanEngine {
class Texture {
 private:
  std::shared_ptr<ImageView> _imageView;
  std::shared_ptr<Sampler> _sampler;

 public:
  Texture(std::shared_ptr<BufferImage> data,
          VkFormat format,
          VkSamplerAddressMode mode,
          int mipMapLevels,
          std::shared_ptr<CommandBuffer> commandBufferTransfer,
          std::shared_ptr<State> state);
  Texture(VkSamplerAddressMode mode,
          int mipMapLevels,
          std::shared_ptr<ImageView> imageView,
          std::shared_ptr<State> state);
  std::shared_ptr<ImageView> getImageView();
  std::shared_ptr<Sampler> getSampler();
};
}  // namespace VulkanEngine