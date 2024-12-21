#pragma once
#include "Utility/EngineState.h"
#include "Graphic/Texture.h"

class Cubemap {
 private:
  std::shared_ptr<EngineState> _engineState;
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
          VkFilter filter,
          std::shared_ptr<CommandBuffer> commandBufferTransfer,
          std::shared_ptr<EngineState> engineState);
  Cubemap(std::tuple<int, int> resolution,
          VkFormat format,
          int mipMapLevels,
          VkImageLayout layout,
          VkImageAspectFlagBits colorBits,
          VkImageUsageFlags usage,
          VkFilter filter,
          std::shared_ptr<CommandBuffer> commandBufferTransfer,
          std::shared_ptr<EngineState> engineState);
  std::shared_ptr<Texture> getTexture();
  std::vector<std::vector<std::shared_ptr<Texture>>> getTextureSeparate();
};