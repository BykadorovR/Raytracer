#pragma once
#include "Utility/EngineState.h"
#include "Utility/Loader.h"
#include "Graphic/Texture.h"
#include "Primitive/Cubemap.h"

class ResourceManager {
 private:
  std::shared_ptr<LoaderGLTF> _loaderGLTF;
  std::shared_ptr<LoaderImage> _loaderImage;
  std::shared_ptr<Texture> _stubTextureZero, _stubTextureOne;
  std::shared_ptr<Cubemap> _stubCubemapZero, _stubCubemapOne;
  std::shared_ptr<EngineState> _engineState;
#ifdef __ANDROID__
  AAssetManager* _assetManager;
#endif
 public:
  ResourceManager(std::shared_ptr<EngineState> engineState);
  void initialize(std::shared_ptr<CommandBuffer> commandBufferTransfer);
#ifdef __ANDROID__
  void setAssetManager(AAssetManager* assetManager);
#endif
  // vector is required to load cubemap
  template <class T>
  std::shared_ptr<BufferImage> loadImageGPU(std::vector<std::shared_ptr<ImageCPU<T>>> images) {
    return _loaderImage->loadGPU<T>(images);
  }

  template <class T>
  std::shared_ptr<ImageCPU<T>> loadImageCPU(std::string path) {
    return _loaderImage->loadCPU<T>(path);
  }
  std::shared_ptr<ModelGLTF> loadModel(std::string path, std::shared_ptr<CommandBuffer> commandBufferTransfer);
  std::shared_ptr<Texture> getTextureZero();
  std::shared_ptr<Texture> getTextureOne();
  std::shared_ptr<Cubemap> getCubemapZero();
  std::shared_ptr<Cubemap> getCubemapOne();
};