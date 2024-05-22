#pragma once
#include "State.h"
#include "Texture.h"
#include "Cubemap.h"
#include "Loader.h"

class ResourceManager {
 private:
  std::shared_ptr<LoaderGLTF> _loaderGLTF;
  std::shared_ptr<LoaderImage> _loaderImage;
  std::shared_ptr<Texture> _stubTextureZero, _stubTextureOne;
  std::shared_ptr<Cubemap> _stubCubemapZero, _stubCubemapOne;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;
  std::shared_ptr<State> _state;
#ifdef __ANDROID__
  AAssetManager* _assetManager;
  std::string _assetEnginePath = "";
#else
  std::string _assetEnginePath = "assets/";
#endif
 public:
  ResourceManager(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state);
  void initialize();
#ifdef __ANDROID__
  void setAssetManager(AAssetManager* assetManager);
#endif
  std::string getAssetEnginePath();
  // vector is required to load cubemap
  std::shared_ptr<BufferImage> loadImageGPU(std::vector<std::string> paths);
  std::tuple<std::shared_ptr<uint8_t[]>, std::tuple<int, int, int>> loadImageCPU(std::string path);
  std::shared_ptr<ModelGLTF> loadModel(std::string path);
  std::shared_ptr<Texture> getTextureZero();
  std::shared_ptr<Texture> getTextureOne();
  std::shared_ptr<Cubemap> getCubemapZero();
  std::shared_ptr<Cubemap> getCubemapOne();
};