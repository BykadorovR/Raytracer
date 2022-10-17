#pragma once
#include "Device.h"

class ImageView {
 private:
  VkImageView _imageView;
  VkFormat _imageFormat;

 public:
  ImageView(VkImage& image, VkFormat format, VkImageAspectFlags aspectFlags, std::shared_ptr<Device> device);
  VkImageView& getImageView();
  VkFormat& getImageFormat();
};