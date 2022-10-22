#pragma once
#include "Device.h"
#include "Image.h"
#include "Sampler.h"
#include <string>

class Texture {
 private:
  std::string _path;
  std::shared_ptr<Device> _device;
  std::shared_ptr<Image> _image;
  std::shared_ptr<ImageView> _imageView;
  std::shared_ptr<Sampler> _sampler;

 public:
  Texture(std::string path,
          std::shared_ptr<CommandPool> commandPool,
          std::shared_ptr<Queue> queue,
          std::shared_ptr<Device> device);
  std::shared_ptr<Image> getImage();
  std::shared_ptr<ImageView> getImageView();
  std::shared_ptr<Sampler> getSampler();
};