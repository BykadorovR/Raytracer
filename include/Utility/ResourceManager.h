#pragma once
#include "State.h"
#include "Texture.h"
#include "Cubemap.h"

class ResourceManager {
 private:
  std::shared_ptr<Texture> _stubTextureZero, _stubTextureOne;
  std::shared_ptr<Cubemap> _stubCubemapZero, _stubCubemapOne;

 public:
  ResourceManager(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state);
  std::shared_ptr<Texture> getTextureZero();
  std::shared_ptr<Texture> getTextureOne();
  std::shared_ptr<Cubemap> getCubemapZero();
  std::shared_ptr<Cubemap> getCubemapOne();
};