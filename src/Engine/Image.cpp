#include "Image.h"

Image::Image(VkImage& image, std::tuple<int, int> resolution, VkFormat format, std::shared_ptr<Device> device) {
  _image = image;
  _device = device;
  _resolution = resolution;
  _format = format;

  _external = true;
}

Image::Image(std::tuple<int, int> resolution,
             int layers,
             VkFormat format,
             VkImageTiling tiling,
             VkImageUsageFlags usage,
             VkMemoryPropertyFlags properties,
             std::shared_ptr<Device> device) {
  _device = device;
  _resolution = resolution;
  _format = format;
  _layers = layers;

  _imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = std::get<0>(resolution);
  imageInfo.extent.height = std::get<1>(resolution);
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = layers;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = _imageLayout;
  imageInfo.usage = usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  // for cubemap need to set flag
  if (layers == 6) imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

  if (vkCreateImage(device->getLogicalDevice(), &imageInfo, nullptr, &_image) != VK_SUCCESS) {
    throw std::runtime_error("failed to create image!");
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(device->getLogicalDevice(), _image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;

  allocInfo.memoryTypeIndex = -1;
  // TODO: findMemoryType to device
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(device->getPhysicalDevice(), &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((memRequirements.memoryTypeBits & (1 << i)) &&
        (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      allocInfo.memoryTypeIndex = i;
    }
  }
  if (allocInfo.memoryTypeIndex < 0) throw std::runtime_error("failed to find suitable memory type!");

  if (vkAllocateMemory(device->getLogicalDevice(), &allocInfo, nullptr, &_imageMemory) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate image memory!");
  }

  vkBindImageMemory(device->getLogicalDevice(), _image, _imageMemory, 0);
}

int Image::getLayersNumber() { return _layers; }

VkFormat& Image::getFormat() { return _format; }

void Image::overrideLayout(VkImageLayout layout) { _imageLayout = layout; }

void Image::changeLayout(VkImageLayout oldLayout,
                         VkImageLayout newLayout,
                         int layersNumber,
                         std::shared_ptr<CommandPool> commandPool,
                         std::shared_ptr<Queue> queue) {
  _imageLayout = newLayout;

  auto commandBuffer = std::make_shared<CommandBuffer>(1, commandPool, _device);
  commandBuffer->beginSingleTimeCommands(0);

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;

  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = _image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = layersNumber;

  VkPipelineStageFlags sourceStage;
  VkPipelineStageFlags destinationStage;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
    barrier.srcAccessMask = 0;
    sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    throw std::invalid_argument("unsupported layout transition!");
  }

  vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[0], sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr,
                       1, &barrier);

  commandBuffer->endSingleTimeCommands(0, queue);
}

void Image::copyFrom(std::shared_ptr<Buffer> buffer,
                     int layersNumber,
                     std::shared_ptr<CommandPool> commandPool,
                     std::shared_ptr<Queue> queue) {
  auto commandBuffer = std::make_shared<CommandBuffer>(1, commandPool, _device);
  commandBuffer->beginSingleTimeCommands(0);
  std::vector<VkBufferImageCopy> bufferCopyRegions;
  for (int i = 0; i < layersNumber; i++) {
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = i;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {(uint32_t)std::get<0>(_resolution), (uint32_t)std::get<1>(_resolution), 1};
    bufferCopyRegions.push_back(region);
  }
  vkCmdCopyBufferToImage(commandBuffer->getCommandBuffer()[0], buffer->getData(), _image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, bufferCopyRegions.size(), bufferCopyRegions.data());

  commandBuffer->endSingleTimeCommands(0, queue);
}

VkImageLayout& Image::getImageLayout() { return _imageLayout; }

VkImage& Image::getImage() { return _image; }

Image::~Image() {
  if (_external == false) {
    vkDestroyImage(_device->getLogicalDevice(), _image, nullptr);
    vkFreeMemory(_device->getLogicalDevice(), _imageMemory, nullptr);
  }
}

ImageView::ImageView(std::shared_ptr<Image> image,
                     VkImageViewType type,
                     int layerCount,
                     int baseArrayLayer,
                     VkImageAspectFlags aspectFlags,
                     std::shared_ptr<Device> device) {
  _device = device;
  _image = image;

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image->getImage();
  viewInfo.viewType = type;
  viewInfo.format = image->getFormat();
  viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.subresourceRange.aspectMask = aspectFlags;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
  viewInfo.subresourceRange.layerCount = layerCount;
  viewInfo.subresourceRange.levelCount = 1;

  if (vkCreateImageView(device->getLogicalDevice(), &viewInfo, nullptr, &_imageView) != VK_SUCCESS) {
    throw std::runtime_error("failed to create texture image view!");
  }
}

VkImageView& ImageView::getImageView() { return _imageView; }

std::shared_ptr<Image> ImageView::getImage() { return _image; }

ImageView::~ImageView() { vkDestroyImageView(_device->getLogicalDevice(), _imageView, nullptr); }