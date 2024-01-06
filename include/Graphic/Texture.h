#pragma once
#include "Image.h"
#include "Sampler.h"
#include "State.h"
#include <string>
#include "stb_image.h"

class Texture {
 private:
  std::string _path;
  std::shared_ptr<ImageView> _imageView;
  std::shared_ptr<Sampler> _sampler;

 public:
  Texture(std::string path,
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