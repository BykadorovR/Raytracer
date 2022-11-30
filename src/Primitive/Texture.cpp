#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "Texture.h"
#include "Buffer.h"

Texture::Texture(std::string path,
                 std::shared_ptr<CommandPool> commandPool,
                 std::shared_ptr<Queue> queue,
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
      std::tuple{texWidth, texHeight}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, device);
  image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandPool, queue);
  image->copyFrom(stagingBuffer, commandPool, queue);
  image->changeLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, commandPool, queue);
  // image view
  _imageView = std::make_shared<ImageView>(image, VK_IMAGE_ASPECT_COLOR_BIT, device);

  _sampler = std::make_shared<Sampler>(device);
}

Texture::Texture(std::shared_ptr<ImageView> imageView, std::shared_ptr<Device> device) {
  _imageView = imageView;
  _sampler = std::make_shared<Sampler>(device);
}

std::shared_ptr<ImageView> Texture::getImageView() { return _imageView; }

std::shared_ptr<Sampler> Texture::getSampler() { return _sampler; }