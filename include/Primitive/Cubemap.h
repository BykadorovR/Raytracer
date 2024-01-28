#pragma once
#include "State.h"
#include "Texture.h"

class Cubemap {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Image> _image;
  std::shared_ptr<ImageView> _imageView;
  // for each face, mip maps are stored
  std::vector<std::vector<std::shared_ptr<ImageView>>> _imageViewSeparate;
  std::shared_ptr<Texture> _texture;
  std::vector<std::vector<std::shared_ptr<Texture>>> _textureSeparate;

 public:
  Cubemap(std::shared_ptr<BufferImage> data,
          VkFormat format,
          int mipMapLevels,
          VkImageAspectFlagBits colorBits,
          VkImageUsageFlags usage,
          std::shared_ptr<CommandBuffer> commandBufferTransfer,
          std::shared_ptr<State> state);
  Cubemap(std::tuple<int, int> resolution,
          VkFormat format,
          int mipMapLevels,
          VkImageLayout layout,
          VkImageAspectFlagBits colorBits,
          VkImageUsageFlags usage,
          std::shared_ptr<CommandBuffer> commandBufferTransfer,
          std::shared_ptr<State> state);
  std::shared_ptr<Texture> getTexture();
  std::vector<std::vector<std::shared_ptr<Texture>>> getTextureSeparate();
};