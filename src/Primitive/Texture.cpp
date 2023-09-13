#define STB_IMAGE_IMPLEMENTATION
#include "Texture.h"
#include "Buffer.h"

void generateMipmaps(std::shared_ptr<Image> image, int mipMapLevels, std::shared_ptr<CommandBuffer> commandBuffer) {
  commandBuffer->beginCommands(0);

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.image = image->getImage();
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  auto [mipWidth, mipHeight] = image->getResolution();
  for (uint32_t i = 1; i < mipMapLevels; i++) {
    // change layout of source image to SRC so we can resize it and generate i-th mip map level
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[0], VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkImageBlit blit{};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {mipWidth, mipHeight, 1};  // previous image size
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = i - 1;  // previous mip map image used for resize
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = i;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;  // number of visible mip map levels (only i here)

    // i = 0 has SRC layout (we changed it above), i = 1 has DST layout (we changed from udefined to dst in constructor)
    vkCmdBlitImage(commandBuffer->getCommandBuffer()[0], image->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   image->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

    // change i = 0 to READ OPTIMAL, we won't use this level anymore, next resizes will use next i
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[0], VK_PIPELINE_STAGE_TRANSFER_BIT,
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

  vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[0], VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

  commandBuffer->endCommands();
  commandBuffer->submitToQueue(true);
  // we changed real image layout above, need to override imageLayout internal field
  image->overrideLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

Texture::Texture(std::string path,
                 VkFormat format,
                 VkSamplerAddressMode mode,
                 int mipMapLevels,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<Settings> settings,
                 std::shared_ptr<Device> device) {
  _device = device;
  // load texture
  int texWidth, texHeight, texChannels;
  stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
  VkDeviceSize imageSize = texWidth * texHeight * 4;

  if (!pixels) {
    throw std::runtime_error("failed to load texture image!");
  }
  // fill buffer
  auto stagingBuffer = std::make_shared<Buffer>(
      imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, device);
  void* data;
  vkMapMemory(device->getLogicalDevice(), stagingBuffer->getMemory(), 0, imageSize, 0, &data);
  memcpy(data, pixels, static_cast<size_t>(imageSize));
  vkUnmapMemory(device->getLogicalDevice(), stagingBuffer->getMemory());

  stbi_image_free(pixels);
  // image
  auto image = std::make_shared<Image>(
      std::tuple{texWidth, texHeight}, 1, mipMapLevels, format, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, device);
  image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 1,
                      mipMapLevels, commandBufferTransfer);
  image->copyFrom(stagingBuffer, 1, commandBufferTransfer);
  // TODO: generate mipmaps here
  // TODO: use commandBuffer with graphic support for Blit (!)
  generateMipmaps(image, mipMapLevels, commandBufferTransfer);

  // image view
  _imageView = std::make_shared<ImageView>(image, VK_IMAGE_VIEW_TYPE_2D, 1, 0, mipMapLevels, VK_IMAGE_ASPECT_COLOR_BIT,
                                           device);
  _sampler = std::make_shared<Sampler>(mode, mipMapLevels, settings->getAnisotropicSamples(), device);
}

Texture::Texture(VkSamplerAddressMode mode, std::shared_ptr<ImageView> imageView, std::shared_ptr<Device> device) {
  _device = device;
  _imageView = imageView;
  _sampler = std::make_shared<Sampler>(mode, 1, 0, device);
}

std::shared_ptr<ImageView> Texture::getImageView() { return _imageView; }

std::shared_ptr<Sampler> Texture::getSampler() { return _sampler; }