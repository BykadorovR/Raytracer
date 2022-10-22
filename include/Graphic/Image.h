#pragma once
#include "Device.h"
#include "Buffer.h"
#include <tuple>

class Image {
 private:
  std::shared_ptr<Device> _device;
  std::tuple<int, int> _resolution;
  VkImage _image;
  VkDeviceMemory _imageMemory;
  VkFormat _format;

 public:
  Image(std::tuple<int, int> resolution,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        std::shared_ptr<Device> device);

  void copyFrom(std::shared_ptr<Buffer> buffer, std::shared_ptr<CommandPool> commandPool, std::shared_ptr<Queue> queue);
  void changeLayout(VkImageLayout oldLayout,
                    VkImageLayout newLayout,
                    std::shared_ptr<CommandPool> commandPool,
                    std::shared_ptr<Queue> queue);
  VkImage& getImage();
  VkDeviceMemory& getImageMemory();

  ~Image();
};

class ImageView {
 private:
  VkImageView _imageView;
  VkFormat _imageFormat;
  std::shared_ptr<Device> _device;

 public:
  ImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, std::shared_ptr<Device> device);
  VkImageView& getImageView();
  VkFormat& getImageFormat();

  ~ImageView();
};