#include "Image.h"

ImageView::ImageView(VkImage& image, VkFormat format, VkImageAspectFlags aspectFlags, std::shared_ptr<Device> device) {
  _imageFormat = format;

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = format;
  viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.subresourceRange.aspectMask = aspectFlags;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(device->getLogicalDevice(), &viewInfo, nullptr, &_imageView) != VK_SUCCESS) {
    throw std::runtime_error("failed to create texture image view!");
  }
}

VkImageView& ImageView::getImageView() { return _imageView; }

VkFormat& ImageView::getImageFormat() { return _imageFormat; }