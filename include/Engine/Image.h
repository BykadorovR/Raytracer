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
  bool _external = false;
  VkImageLayout _imageLayout;

 public:
  Image(VkImage& image, std::tuple<int, int> resolution, VkFormat format, std::shared_ptr<Device> device);
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
  void overrideLayout(VkImageLayout layout);
  VkImage& getImage();
  VkFormat& getFormat();
  VkImageLayout& getImageLayout();

  ~Image();
};

class ImageView {
 private:
  std::shared_ptr<Image> _image;
  VkImageView _imageView;
  std::shared_ptr<Device> _device;

 public:
  ImageView(std::shared_ptr<Image> image, VkImageAspectFlags aspectFlags, std::shared_ptr<Device> device);
  VkImageView& getImageView();
  std::shared_ptr<Image> getImage();

  ~ImageView();
};