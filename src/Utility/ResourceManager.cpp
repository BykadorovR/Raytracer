#include "ResourceManager.h"

ResourceManager::ResourceManager(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state) {
  _commandBufferTransfer = commandBufferTransfer;
  _state = state;
}

void ResourceManager::initialize() {
  _loaderImage = std::make_shared<LoaderImage>(_commandBufferTransfer, _state);
  _loaderGLTF = std::make_shared<LoaderGLTF>(_commandBufferTransfer, _loaderImage, _state);
#ifdef __ANDROID__
  _loaderGLTF->setAssetManager(_assetManager);
#endif
  _stubTextureOne = std::make_shared<Texture>(loadImageGPU<uint8_t>({_assetEnginePath + "stubs/Texture1x1.png"}),
                                              _state->getSettings()->getLoadTextureColorFormat(),
                                              VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, _commandBufferTransfer, _state);
  _stubTextureZero = std::make_shared<Texture>(loadImageGPU<uint8_t>({_assetEnginePath + "stubs/Texture1x1Black.png"}),
                                               _state->getSettings()->getLoadTextureColorFormat(),
                                               VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, _commandBufferTransfer, _state);

  _stubCubemapZero = std::make_shared<Cubemap>(
      loadImageGPU<uint8_t>(std::vector<std::string>{
          _assetEnginePath + "stubs/Texture1x1Black.png", _assetEnginePath + "stubs/Texture1x1Black.png",
          _assetEnginePath + "stubs/Texture1x1Black.png", _assetEnginePath + "stubs/Texture1x1Black.png",
          _assetEnginePath + "stubs/Texture1x1Black.png", _assetEnginePath + "stubs/Texture1x1Black.png"}),
      _state->getSettings()->getLoadTextureColorFormat(), 1, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, _commandBufferTransfer, _state);

  _stubCubemapOne = std::make_shared<Cubemap>(
      loadImageGPU<uint8_t>(std::vector<std::string>{
          _assetEnginePath + "stubs/Texture1x1.png", _assetEnginePath + "stubs/Texture1x1.png",
          _assetEnginePath + "stubs/Texture1x1.png", _assetEnginePath + "stubs/Texture1x1.png",
          _assetEnginePath + "stubs/Texture1x1.png", _assetEnginePath + "stubs/Texture1x1.png"}),
      _state->getSettings()->getLoadTextureColorFormat(), 1, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, _commandBufferTransfer, _state);
}

std::string ResourceManager::getAssetEnginePath() { return _assetEnginePath; }

#ifdef __ANDROID__
void ResourceManager::setAssetManager(AAssetManager* assetManager) { _assetManager = assetManager; }
#endif

std::shared_ptr<ModelGLTF> ResourceManager::loadModel(std::string path) { return _loaderGLTF->load(path); }

std::shared_ptr<Texture> ResourceManager::getTextureZero() { return _stubTextureZero; }

std::shared_ptr<Texture> ResourceManager::getTextureOne() { return _stubTextureOne; }

std::shared_ptr<Cubemap> ResourceManager::getCubemapZero() { return _stubCubemapZero; }

std::shared_ptr<Cubemap> ResourceManager::getCubemapOne() { return _stubCubemapOne; }