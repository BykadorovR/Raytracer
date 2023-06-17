#pragma once
#include "Device.h"
#include "Image.h"
#include "Sampler.h"
#include <string>
#include "stb_image.h"

class Texture {
 private:
  std::string _path;
  std::shared_ptr<Device> _device;
  std::shared_ptr<ImageView> _imageView;
  std::shared_ptr<Sampler> _sampler;

 public:
  Texture(std::string path,
          VkSamplerAddressMode mode,
          std::shared_ptr<CommandBuffer> commandBufferTransfer,
          std::shared_ptr<Device> device);
  Texture(VkSamplerAddressMode mode, std::shared_ptr<ImageView> imageView, std::shared_ptr<Device> device);
  std::shared_ptr<ImageView> getImageView();
  std::shared_ptr<Sampler> getSampler();
};