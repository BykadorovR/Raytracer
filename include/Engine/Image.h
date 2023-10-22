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
  int _layers;
  bool _external = false;
  VkImageLayout _imageLayout;
  std::shared_ptr<Buffer> _stagingBuffer;

 public:
  Image(VkImage& image, std::tuple<int, int> resolution, VkFormat format, std::shared_ptr<Device> device);
  Image(std::tuple<int, int> resolution,
        int layers,
        int mipMapLevels,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        std::shared_ptr<Device> device);

  // bufferOffsets contains offsets for part of buffer that should be copied to corresponding layers of image
  void copyFrom(std::shared_ptr<Buffer> buffer,
                std::vector<int> bufferOffsets,
                std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void changeLayout(VkImageLayout oldLayout,
                    VkImageLayout newLayout,
                    VkImageAspectFlags aspectMask,
                    int layersNumber,
                    int mipMapLevels,
                    std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void overrideLayout(VkImageLayout layout);
  std::tuple<int, int> getResolution();
  VkImage& getImage();
  VkFormat& getFormat();
  VkImageLayout& getImageLayout();
  int getLayersNumber();

  ~Image();
};

class ImageView {
 private:
  std::shared_ptr<Image> _image;
  VkImageView _imageView;
  std::shared_ptr<Device> _device;

 public:
  // layerCount = 1, baseArrayLayer = 0 by default
  // type VK_IMAGE_VIEW_TYPE_2D or VK_IMAGE_VIEW_TYPE_CUBE
  ImageView(std::shared_ptr<Image> image,
            VkImageViewType type,
            int layerCount,
            int baseArrayLayer,
            int mipMapLevels,
            VkImageAspectFlags aspectFlags,
            std::shared_ptr<Device> device);
  VkImageView& getImageView();
  std::shared_ptr<Image> getImage();

  ~ImageView();
};