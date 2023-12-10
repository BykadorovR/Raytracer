#include "Equirectangular.h"

Equirectangular::Equirectangular(std::string path,
                                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                                 std::shared_ptr<State> state) {
  float* pixels;
  int texWidth, texHeight, texChannels;
  pixels = stbi_loadf(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
  if (!pixels) {
    throw std::runtime_error("failed to load texture image!");
  }

  int imageSize = texWidth * texHeight * STBI_rgb_alpha;
  int bufferSize = imageSize * sizeof(float);
  // fill buffer
  _stagingBuffer = std::make_shared<Buffer>(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                            state->getDevice());
  void* data;
  vkMapMemory(state->getDevice()->getLogicalDevice(), _stagingBuffer->getMemory(), 0, bufferSize, 0, &data);
  memcpy((stbi_uc*)data, pixels, static_cast<size_t>(bufferSize));
  vkUnmapMemory(state->getDevice()->getLogicalDevice(), _stagingBuffer->getMemory());
  stbi_image_free(pixels);

  // image
  auto [width, height] = state->getSettings()->getResolution();
  _image = std::make_shared<Image>(std::tuple{texWidth, texHeight}, 1, 1, state->getSettings()->getGraphicColorFormat(),
                                   VK_IMAGE_TILING_OPTIMAL,
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->getDevice());
  _image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                       commandBufferTransfer);
  _image->copyFrom(_stagingBuffer, {0}, commandBufferTransfer);
  _image->changeLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, commandBufferTransfer);
  _imageView = std::make_shared<ImageView>(_image, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT,
                                           state->getDevice());
  _texture = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, _imageView, state);
}

std::shared_ptr<Texture> Equirectangular::getTexture() { return _texture; }
