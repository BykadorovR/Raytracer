#pragma once
#include "Image.h"
#include "Sampler.h"
#include "State.h"
#include <string>
#include "stb_image.h"

class Texture {
 private:
  std::shared_ptr<ImageView> _imageView;
  std::shared_ptr<Sampler> _sampler;

 public:
  Texture(std::shared_ptr<BufferImage> data,
          VkFormat format,
          VkSamplerAddressMode mode,
          int mipMapLevels,
          VkFilter filter,
          std::shared_ptr<CommandBuffer> commandBufferTransfer,
          std::shared_ptr<State> state);
  Texture(VkSamplerAddressMode mode,
          int mipMapLevels,
          VkFilter filter,
          std::shared_ptr<ImageView> imageView,
          std::shared_ptr<State> state);
  std::shared_ptr<ImageView> getImageView();
  std::shared_ptr<Sampler> getSampler();
};