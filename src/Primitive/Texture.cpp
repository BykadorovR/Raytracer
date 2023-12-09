#define STB_IMAGE_IMPLEMENTATION
#include "Texture.h"
#include "Buffer.h"

Texture::Texture(std::string path,
                 VkFormat format,
                 VkSamplerAddressMode mode,
                 int mipMapLevels,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<State> state) {
  _path = path;
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
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, state->getDevice());
  void* data;
  vkMapMemory(state->getDevice()->getLogicalDevice(), stagingBuffer->getMemory(), 0, imageSize, 0, &data);
  memcpy(data, pixels, static_cast<size_t>(imageSize));
  vkUnmapMemory(state->getDevice()->getLogicalDevice(), stagingBuffer->getMemory());

  stbi_image_free(pixels);
  // image
  auto image = std::make_shared<Image>(
      std::tuple{texWidth, texHeight}, 1, mipMapLevels, format, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->getDevice());
  image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 1,
                      mipMapLevels, commandBufferTransfer);
  image->copyFrom(stagingBuffer, {0}, commandBufferTransfer);
  // TODO: generate mipmaps here
  // TODO: use commandBuffer with graphic support for Blit (!)
  image->generateMipmaps(mipMapLevels, commandBufferTransfer);

  // image view
  _imageView = std::make_shared<ImageView>(image, VK_IMAGE_VIEW_TYPE_2D, 1, 0, mipMapLevels, VK_IMAGE_ASPECT_COLOR_BIT,
                                           state->getDevice());
  _sampler = std::make_shared<Sampler>(mode, mipMapLevels, state->getSettings()->getAnisotropicSamples(),
                                       state->getDevice());
}

Texture::Texture(VkSamplerAddressMode mode,
                 int mipMapLevels,
                 std::shared_ptr<ImageView> imageView,
                 std::shared_ptr<State> state) {
  _imageView = imageView;
  _sampler = std::make_shared<Sampler>(mode, mipMapLevels, state->getSettings()->getAnisotropicSamples(),
                                       state->getDevice());
}

std::shared_ptr<ImageView> Texture::getImageView() { return _imageView; }

std::shared_ptr<Sampler> Texture::getSampler() { return _sampler; }