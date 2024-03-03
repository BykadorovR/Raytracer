module ResourceManager;

namespace VulkanEngine {
ResourceManager::ResourceManager(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state) {
  _loaderImage = std::make_shared<LoaderImage>(commandBufferTransfer, state);
  _loaderGLTF = std::make_shared<LoaderGLTF>(commandBufferTransfer, _loaderImage, state);

  _stubTextureOne = std::make_shared<Texture>(loadImage({"assets/stubs/Texture1x1.png"}),
                                              state->getSettings()->getLoadTextureColorFormat(),
                                              VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer, state);
  _stubTextureZero = std::make_shared<Texture>(loadImage({"assets/stubs/Texture1x1Black.png"}),
                                               state->getSettings()->getLoadTextureColorFormat(),
                                               VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer, state);

  _stubCubemapZero = std::make_shared<Cubemap>(
      loadImage(std::vector<std::string>{"assets/stubs/Texture1x1Black.png", "assets/stubs/Texture1x1Black.png",
                                         "assets/stubs/Texture1x1Black.png", "assets/stubs/Texture1x1Black.png",
                                         "assets/stubs/Texture1x1Black.png", "assets/stubs/Texture1x1Black.png"}),
      state->getSettings()->getLoadTextureColorFormat(), 1, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);

  _stubCubemapOne = std::make_shared<Cubemap>(
      loadImage(std::vector<std::string>{"assets/stubs/Texture1x1.png", "assets/stubs/Texture1x1.png",
                                         "assets/stubs/Texture1x1.png", "assets/stubs/Texture1x1.png",
                                         "assets/stubs/Texture1x1.png", "assets/stubs/Texture1x1.png"}),
      state->getSettings()->getLoadTextureColorFormat(), 1, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);
}

std::shared_ptr<BufferImage> ResourceManager::loadImage(std::vector<std::string> paths) {
  return _loaderImage->load(paths);
}

std::shared_ptr<ModelGLTF> ResourceManager::loadModel(std::string path) { return _loaderGLTF->load(path); }

std::shared_ptr<Texture> ResourceManager::getTextureZero() { return _stubTextureZero; }

std::shared_ptr<Texture> ResourceManager::getTextureOne() { return _stubTextureOne; }

std::shared_ptr<Cubemap> ResourceManager::getCubemapZero() { return _stubCubemapZero; }

std::shared_ptr<Cubemap> ResourceManager::getCubemapOne() { return _stubCubemapOne; }
}  // namespace VulkanEngine