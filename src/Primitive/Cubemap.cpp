#include "Cubemap.h"
#include "Buffer.h"

Cubemap::Cubemap(std::string path, std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state) {
  _state = state;
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
  auto [width, height] = state->getSettings()->getResolution();
  _image = std::make_shared<Image>(std::tuple{texWidth, texHeight}, 6, VK_FORMAT_R8G8B8A8_UNORM,
                                   VK_IMAGE_TILING_OPTIMAL,
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->getDevice());
  _image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 6,
                       commandBufferTransfer);
  _image->copyFrom(stagingBuffer, 6, commandBufferTransfer);
  _image->changeLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_IMAGE_ASPECT_COLOR_BIT, 6, commandBufferTransfer);
  _imageView = std::make_shared<ImageView>(_image, VK_IMAGE_VIEW_TYPE_CUBE, 6, 0, VK_IMAGE_ASPECT_COLOR_BIT,
                                           state->getDevice());
  _texture = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, _imageView, _state->getDevice());
}

Cubemap::Cubemap(std::tuple<int, int> resolution,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<State> state) {
  _state = state;
  _image = std::make_shared<Image>(resolution, 6, _state->getSettings()->getDepthFormat(), VK_IMAGE_TILING_OPTIMAL,
                                   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->getDevice());
  _image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                       VK_IMAGE_ASPECT_DEPTH_BIT, 6, commandBufferTransfer);

  _imageView = std::make_shared<ImageView>(_image, VK_IMAGE_VIEW_TYPE_CUBE, 6, 0, VK_IMAGE_ASPECT_DEPTH_BIT,
                                           state->getDevice());
  _texture = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, _imageView, _state->getDevice());

  _imageViewSeparate.resize(6);
  _textureSeparate.resize(6);
  for (int i = 0; i < 6; i++) {
    _imageViewSeparate[i] = std::make_shared<ImageView>(_image, VK_IMAGE_VIEW_TYPE_2D, 1, i, VK_IMAGE_ASPECT_DEPTH_BIT,
                                                        state->getDevice());
    _textureSeparate[i] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, _imageViewSeparate[i],
                                                    _state->getDevice());
  }
}

std::shared_ptr<Texture> Cubemap::getTexture() { return _texture; }

std::vector<std::shared_ptr<Texture>> Cubemap::getTextureSeparate() { return _textureSeparate; }