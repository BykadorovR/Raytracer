#include "ResourceManager.h"

ResourceManager::ResourceManager(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state) {
  _commandBufferTransfer = commandBufferTransfer;
  _state = state;

  _stubTextureOne = std::make_shared<Texture>(load({"assets/stubs/Texture1x1.png"}),
                                              state->getSettings()->getLoadTextureColorFormat(),
                                              VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer, state);
  _stubTextureZero = std::make_shared<Texture>(load({"assets/stubs/Texture1x1Black.png"}),
                                               state->getSettings()->getLoadTextureColorFormat(),
                                               VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer, state);

  _stubCubemapZero = std::make_shared<Cubemap>(
      load(std::vector<std::string>{"assets/stubs/Texture1x1Black.png", "assets/stubs/Texture1x1Black.png",
                                    "assets/stubs/Texture1x1Black.png", "assets/stubs/Texture1x1Black.png",
                                    "assets/stubs/Texture1x1Black.png", "assets/stubs/Texture1x1Black.png"}),
      state->getSettings()->getLoadTextureColorFormat(), 1, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);

  _stubCubemapOne = std::make_shared<Cubemap>(
      load(std::vector<std::string>{"assets/stubs/Texture1x1.png", "assets/stubs/Texture1x1.png",
                                    "assets/stubs/Texture1x1.png", "assets/stubs/Texture1x1.png",
                                    "assets/stubs/Texture1x1.png", "assets/stubs/Texture1x1.png"}),
      state->getSettings()->getLoadTextureColorFormat(), 1, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);
}

std::shared_ptr<BufferImage> ResourceManager::load(std::vector<std::string> paths) {
  for (auto& path : paths) {
    if (_images.contains(path) == false) {
      // load texture
      int texWidth, texHeight, texChannels;
      stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
      VkDeviceSize imageSize = texWidth * texHeight * 4;

      if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
      }
      // fill buffer
      _images[path] = std::make_shared<BufferImage>(
          std::tuple{texWidth, texHeight}, 4, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());
      _images[path]->map();
      memcpy(_images[path]->getMappedMemory(), pixels, static_cast<size_t>(imageSize));
      _images[path]->unmap();

      stbi_image_free(pixels);
    }
  }

  std::shared_ptr<BufferImage> bufferDst = _images[paths.front()];
  if (paths.size() > 1) {
    bufferDst = std::make_shared<BufferImage>(
        bufferDst->getResolution(), bufferDst->getChannels(), paths.size(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());
    for (int i = 0; i < paths.size(); i++) {
      auto bufferSrc = _images[paths[i]];
      bufferDst->copyFrom(bufferSrc, 0, i * bufferSrc->getSize(), _commandBufferTransfer);
    }
    // barrier for further image copy from buffer
    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.pNext = nullptr;
    memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(_commandBufferTransfer->getCommandBuffer()[_commandBufferTransfer->getCurrentFrame()],
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &memoryBarrier, 0,
                         nullptr, 0, nullptr);
  }

  return bufferDst;
}

std::shared_ptr<Texture> ResourceManager::getTextureZero() { return _stubTextureZero; }

std::shared_ptr<Texture> ResourceManager::getTextureOne() { return _stubTextureOne; }

std::shared_ptr<Cubemap> ResourceManager::getCubemapZero() { return _stubCubemapZero; }

std::shared_ptr<Cubemap> ResourceManager::getCubemapOne() { return _stubCubemapOne; }