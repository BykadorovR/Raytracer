#include "Vulkan/Image.h"

Image::Image(VkImage& image,
             std::tuple<int, int> resolution,
             VkFormat format,
             std::shared_ptr<EngineState> engineState) {
  _image = image;
  _engineState = engineState;
  _resolution = resolution;
  _format = format;

  _external = true;
}

Image::Image(std::tuple<int, int> resolution,
             int layers,
             int mipMapLevels,
             VkFormat format,
             VkImageTiling tiling,
             VkImageUsageFlags usage,
             VkMemoryPropertyFlags properties,
             std::shared_ptr<EngineState> engineState) {
  _engineState = engineState;
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
  imageInfo.mipLevels = mipMapLevels;
  imageInfo.arrayLayers = layers;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = _imageLayout;
  imageInfo.usage = usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  // for cubemap need to set flag
  if (layers == 6) imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

  if (vkCreateImage(_engineState->getDevice()->getLogicalDevice(), &imageInfo, nullptr, &_image) != VK_SUCCESS) {
    throw std::runtime_error("failed to create image!");
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(_engineState->getDevice()->getLogicalDevice(), _image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;

  allocInfo.memoryTypeIndex = -1;
  // TODO: findMemoryType to device
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(_engineState->getDevice()->getPhysicalDevice(), &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((memRequirements.memoryTypeBits & (1 << i)) &&
        (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      allocInfo.memoryTypeIndex = i;
      break;
    }
  }
  if (allocInfo.memoryTypeIndex < 0) throw std::runtime_error("failed to find suitable memory type!");

  if (vkAllocateMemory(_engineState->getDevice()->getLogicalDevice(), &allocInfo, nullptr, &_imageMemory) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to allocate image memory!");
  }

  vkBindImageMemory(_engineState->getDevice()->getLogicalDevice(), _image, _imageMemory, 0);
}

int Image::getLayersNumber() { return _layers; }

VkFormat& Image::getFormat() { return _format; }

void Image::overrideLayout(VkImageLayout layout) { _imageLayout = layout; }

std::tuple<int, int> Image::getResolution() { return _resolution; }

void Image::generateMipmaps(int mipMapLevels, int layers, std::shared_ptr<CommandBuffer> commandBuffer) {
  int currentFrame = _engineState->getFrameInFlight();

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.image = getImage();
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = layers;
  barrier.subresourceRange.levelCount = 1;

  auto [mipWidth, mipHeight] = getResolution();
  for (uint32_t i = 1; i < mipMapLevels; i++) {
    // change layout of source image to SRC so we can resize it and generate i-th mip map level
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkImageBlit blit{};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {mipWidth, mipHeight, 1};  // previous image size
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = i - 1;  // previous mip map image used for resize
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = layers;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = i;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = layers;

    // i = 0 has SRC layout (we changed it above), i = 1 has DST layout (we changed from undefined to dst in
    // constructor)
    vkCmdBlitImage(commandBuffer->getCommandBuffer()[currentFrame], getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

    // change i = 0 to READ OPTIMAL, we won't use this level anymore, next resizes will use next i
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    if (mipWidth > 1) mipWidth /= 2;
    if (mipHeight > 1) mipHeight /= 2;
  }

  // we don't generate mip map from last level so we need explicitly change dst to read only
  barrier.subresourceRange.baseMipLevel = mipMapLevels - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

  // we changed real image layout above, need to override imageLayout internal field
  overrideLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void Image::changeLayout(VkImageLayout oldLayout,
                         VkImageLayout newLayout,
                         VkImageAspectFlags aspectMask,
                         int layersNumber,
                         int mipMapLevels,
                         std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  _imageLayout = newLayout;

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;

  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = _image;
  barrier.subresourceRange.aspectMask = aspectMask;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mipMapLevels;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = layersNumber;

  switch (oldLayout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      // Image layout is undefined (or does not matter)
      // Only valid as initial layout
      // No flags required, listed only for completeness
      barrier.srcAccessMask = 0;
      break;

    case VK_IMAGE_LAYOUT_PREINITIALIZED:
      // Image is preinitialized
      // Only valid as initial layout for linear images, preserves memory contents
      // Make sure host writes have been finished
      barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      // Image is a color attachment
      // Make sure any writes to the color buffer have been finished
      barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      // Image is a depth/stencil attachment
      // Make sure any writes to the depth/stencil buffer have been finished
      barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      // Image is a transfer source
      // Make sure any reads from the image have been finished
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      // Image is a transfer destination
      // Make sure any writes to the image have been finished
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      // Image is read by a shader
      // Make sure any shader reads from the image have been finished
      barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      break;
    default:
      // Other source layouts aren't handled (yet)
      break;
  }

  // Target layouts (new)
  // Destination access mask controls the dependency for the new image layout
  switch (newLayout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      // Image will be used as a transfer destination
      // Make sure any writes to the image have been finished
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      // Image will be used as a transfer source
      // Make sure any reads from the image have been finished
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      break;

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      // Image will be used as a color attachment
      // Make sure any writes to the color buffer have been finished
      barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      break;

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      // Image layout will be used as a depth/stencil attachment
      // Make sure any writes to depth/stencil buffer have been finished
      barrier.dstAccessMask = barrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      // Image will be read in a shader (sampler, input attachment)
      // Make sure any writes to the image have been finished
      if (barrier.srcAccessMask == 0) {
        barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
      }
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      break;
    default:
      // Other source layouts aren't handled (yet)
      break;
  }

  vkCmdPipelineBarrier(commandBufferTransfer->getCommandBuffer()[_engineState->getFrameInFlight()],
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
}

void Image::setData(std::shared_ptr<Buffer> buffer) {}

void Image::copyFrom(std::shared_ptr<Buffer> buffer,
                     std::vector<int> bufferOffsets,
                     std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  _stagingBuffer = buffer;

  std::vector<VkBufferImageCopy> bufferCopyRegions;
  for (int i = 0; i < bufferOffsets.size(); i++) {
    VkBufferImageCopy region{};
    region.bufferOffset = bufferOffsets[i];
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

  int currentFrame = _engineState->getFrameInFlight();
  vkCmdCopyBufferToImage(commandBufferTransfer->getCommandBuffer()[currentFrame], buffer->getData(), _image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, bufferCopyRegions.size(), bufferCopyRegions.data());
  // need to insert memory barrier so read in fragment shader waits for copy
  VkMemoryBarrier memoryBarrier = {};
  memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  memoryBarrier.pNext = nullptr;
  memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(commandBufferTransfer->getCommandBuffer()[currentFrame], VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
}

VkImageLayout& Image::getImageLayout() { return _imageLayout; }

VkImage& Image::getImage() { return _image; }

Image::~Image() {
  if (_external == false) {
    vkDestroyImage(_engineState->getDevice()->getLogicalDevice(), _image, nullptr);
    vkFreeMemory(_engineState->getDevice()->getLogicalDevice(), _imageMemory, nullptr);
  }
}

ImageView::ImageView(std::shared_ptr<Image> image,
                     VkImageViewType type,
                     int baseArrayLayer,
                     int arrayLayerNumber,
                     int baseMipMap,
                     int mipMapNumber,
                     VkImageAspectFlags aspectFlags,
                     std::shared_ptr<EngineState> engineState) {
  _engineState = engineState;
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
  viewInfo.subresourceRange.baseMipLevel = baseMipMap;
  viewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
  viewInfo.subresourceRange.layerCount = arrayLayerNumber;
  viewInfo.subresourceRange.levelCount = mipMapNumber;

  if (vkCreateImageView(_engineState->getDevice()->getLogicalDevice(), &viewInfo, nullptr, &_imageView) != VK_SUCCESS) {
    throw std::runtime_error("failed to create texture image view!");
  }
}

VkImageView& ImageView::getImageView() { return _imageView; }

std::shared_ptr<Image> ImageView::getImage() { return _image; }

ImageView::~ImageView() { vkDestroyImageView(_engineState->getDevice()->getLogicalDevice(), _imageView, nullptr); }