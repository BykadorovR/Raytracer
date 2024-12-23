#include "Utility/ResourceManager.h"

ResourceManager::ResourceManager(std::shared_ptr<EngineState> engineState) { _engineState = engineState; }

void ResourceManager::initialize(std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  _loaderImage = std::make_shared<LoaderImage>(_engineState);
  _loaderGLTF = std::make_shared<LoaderGLTF>(_loaderImage, _engineState);
#ifdef __ANDROID__
  _loaderGLTF->setAssetManager(_assetManager);
#endif
  auto assetPath = _engineState->getSettings()->getAssetsPath();
  _stubTextureOne = std::make_shared<Texture>(
      loadImageGPU<uint8_t>({loadImageCPU<uint8_t>(assetPath + "stubs/Texture1x1.png")}),
      _engineState->getSettings()->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, VK_FILTER_LINEAR,
      commandBufferTransfer, _engineState);
  _stubTextureZero = std::make_shared<Texture>(
      loadImageGPU<uint8_t>({loadImageCPU<uint8_t>(assetPath + "stubs/Texture1x1Black.png")}),
      _engineState->getSettings()->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, VK_FILTER_LINEAR,
      commandBufferTransfer, _engineState);

  _stubCubemapZero = std::make_shared<Cubemap>(
      loadImageGPU<uint8_t>({loadImageCPU<uint8_t>(assetPath + "stubs/Texture1x1Black.png"),
                             loadImageCPU<uint8_t>(assetPath + "stubs/Texture1x1Black.png"),
                             loadImageCPU<uint8_t>(assetPath + "stubs/Texture1x1Black.png"),
                             loadImageCPU<uint8_t>(assetPath + "stubs/Texture1x1Black.png"),
                             loadImageCPU<uint8_t>(assetPath + "stubs/Texture1x1Black.png"),
                             loadImageCPU<uint8_t>(assetPath + "stubs/Texture1x1Black.png")}),
      _engineState->getSettings()->getLoadTextureColorFormat(), 1, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FILTER_LINEAR, commandBufferTransfer,
      _engineState);

  _stubCubemapOne = std::make_shared<Cubemap>(
      loadImageGPU<uint8_t>({loadImageCPU<uint8_t>(assetPath + "stubs/Texture1x1.png"),
                             loadImageCPU<uint8_t>(assetPath + "stubs/Texture1x1.png"),
                             loadImageCPU<uint8_t>(assetPath + "stubs/Texture1x1.png"),
                             loadImageCPU<uint8_t>(assetPath + "stubs/Texture1x1.png"),
                             loadImageCPU<uint8_t>(assetPath + "stubs/Texture1x1.png"),
                             loadImageCPU<uint8_t>(assetPath + "stubs/Texture1x1.png")}),
      _engineState->getSettings()->getLoadTextureColorFormat(), 1, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FILTER_LINEAR, commandBufferTransfer,
      _engineState);
}

#ifdef __ANDROID__
void ResourceManager::setAssetManager(AAssetManager* assetManager) { _assetManager = assetManager; }
#endif

std::shared_ptr<ModelGLTF> ResourceManager::loadModel(std::string path,
                                                      std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  return _loaderGLTF->load(path, commandBufferTransfer);
}

std::shared_ptr<Texture> ResourceManager::getTextureZero() { return _stubTextureZero; }

std::shared_ptr<Texture> ResourceManager::getTextureOne() { return _stubTextureOne; }

std::shared_ptr<Cubemap> ResourceManager::getCubemapZero() { return _stubCubemapZero; }

std::shared_ptr<Cubemap> ResourceManager::getCubemapOne() { return _stubCubemapOne; }