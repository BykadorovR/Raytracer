#pragma once
#include "State.h"

class Cubemap {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Image> _image;
  std::shared_ptr<ImageView> _imageView;
  std::vector<std::shared_ptr<ImageView>> _imageViewSeparate;
  std::shared_ptr<Texture> _texture;
  std::vector<std::shared_ptr<Texture>> _textureSeparate;
  std::shared_ptr<Buffer> _stagingBuffer;

 public:
  Cubemap(std::vector<std::string> path,
          VkFormat format,
          VkImageLayout layout,
          VkImageAspectFlagBits colorBits,
          VkImageUsageFlags usage,
          std::shared_ptr<CommandBuffer> commandBufferTransfer,
          std::shared_ptr<State> state);
  Cubemap(std::tuple<int, int> resolution,
          VkFormat format,
          VkImageLayout layout,
          VkImageAspectFlagBits colorBits,
          VkImageUsageFlags usage,
          std::shared_ptr<CommandBuffer> commandBufferTransfer,
          std::shared_ptr<State> state);
  std::shared_ptr<Texture> getTexture();
  std::vector<std::shared_ptr<Texture>> getTextureSeparate();
};