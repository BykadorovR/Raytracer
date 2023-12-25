#include "Cubemap.h"
#include "Buffer.h"

Cubemap::Cubemap(std::vector<std::string> path,
                 VkFormat format,
                 int mipMapLevels,
                 VkImageLayout layout,
                 VkImageAspectFlagBits colorBits,
                 VkImageUsageFlags usage,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<State> state) {
  _state = state;
  // load texture
  std::vector<stbi_uc*> pixelsCubemap;
  int texWidth, texHeight, texChannels;
  for (auto currentPath : path) {
    stbi_uc* pixels = stbi_load(currentPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) {
      throw std::runtime_error("failed to load texture image!");
    }

    pixelsCubemap.push_back(pixels);
  }

  int imageSize = texWidth * texHeight * STBI_rgb_alpha;
  int bufferSize = imageSize * pixelsCubemap.size();
  // fill buffer
  _stagingBuffer = std::make_shared<Buffer>(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                            state->getDevice());
  void* data;
  vkMapMemory(state->getDevice()->getLogicalDevice(), _stagingBuffer->getMemory(), 0, bufferSize, 0, &data);

  int offset = 0;
  for (int i = 0; i < pixelsCubemap.size(); i++) {
    memcpy((stbi_uc*)data + offset, pixelsCubemap[i], static_cast<size_t>(imageSize));
    offset += imageSize;
  }
  vkUnmapMemory(state->getDevice()->getLogicalDevice(), _stagingBuffer->getMemory());

  for (auto& pixels : pixelsCubemap) stbi_image_free(pixels);

  // image
  auto [width, height] = state->getSettings()->getResolution();
  // usage VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
  _image = std::make_shared<Image>(std::tuple{texWidth, texHeight}, 6, mipMapLevels, format, VK_IMAGE_TILING_OPTIMAL,
                                   usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->getDevice());
  _image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, colorBits, 6, mipMapLevels,
                       commandBufferTransfer);
  _image->copyFrom(_stagingBuffer, {0, imageSize, imageSize * 2, imageSize * 3, imageSize * 4, imageSize * 5},
                   commandBufferTransfer);

  _image->generateMipmaps(mipMapLevels, 6, commandBufferTransfer);

  // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  //_image->changeLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout, colorBits, 6, mipMapLevels,
  // commandBufferTransfer);
  _imageView = std::make_shared<ImageView>(_image, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6, 0, mipMapLevels, colorBits,
                                           state->getDevice());
  _texture = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, mipMapLevels, _imageView, _state);

  _imageViewSeparate.resize(6);
  _textureSeparate.resize(6);
  for (int i = 0; i < 6; i++) {
    _imageViewSeparate[i].resize(mipMapLevels);
    _textureSeparate[i].resize(mipMapLevels);
    for (int j = 0; j < mipMapLevels; j++) {
      _imageViewSeparate[i][j] = std::make_shared<ImageView>(_image, VK_IMAGE_VIEW_TYPE_2D, i, 1, j, 1, colorBits,
                                                             state->getDevice());
      _textureSeparate[i][j] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1,
                                                         _imageViewSeparate[i][j], _state);
    }
  }
}

Cubemap::Cubemap(std::tuple<int, int> resolution,
                 VkFormat format,
                 int mipMapLevels,
                 VkImageLayout layout,
                 VkImageAspectFlagBits colorBits,
                 VkImageUsageFlags usage,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<State> state) {
  _state = state;
  // usage VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
  _image = std::make_shared<Image>(resolution, 6, mipMapLevels, format, VK_IMAGE_TILING_OPTIMAL, usage,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->getDevice());
  // VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
  _image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, layout, colorBits, 6, mipMapLevels, commandBufferTransfer);
  _imageView = std::make_shared<ImageView>(_image, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6, 0, mipMapLevels, colorBits,
                                           state->getDevice());
  _texture = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, mipMapLevels, _imageView, _state);

  _imageViewSeparate.resize(6);
  _textureSeparate.resize(6);
  for (int i = 0; i < 6; i++) {
    _imageViewSeparate[i].resize(mipMapLevels);
    _textureSeparate[i].resize(mipMapLevels);
    for (int j = 0; j < mipMapLevels; j++) {
      _imageViewSeparate[i][j] = std::make_shared<ImageView>(_image, VK_IMAGE_VIEW_TYPE_2D, i, 1, j, 1, colorBits,
                                                             state->getDevice());
      _textureSeparate[i][j] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1,
                                                         _imageViewSeparate[i][j], _state);
    }
  }
}

std::shared_ptr<Texture> Cubemap::getTexture() { return _texture; }

std::vector<std::vector<std::shared_ptr<Texture>>> Cubemap::getTextureSeparate() { return _textureSeparate; }