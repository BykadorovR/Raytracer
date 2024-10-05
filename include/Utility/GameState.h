#pragma once
#include <Utility/State.h>
#include <Vulkan/Command.h>
#include "Utility/ResourceManager.h"
#include "Graphic/CameraManager.h"
#include "Graphic/LightManager.h"

class GameState {
 private:
  std::shared_ptr<ResourceManager> _resourceManager;
  std::shared_ptr<CameraManager> _cameraManager;
  std::shared_ptr<LightManager> _lightManager;

 public:
  GameState(std::shared_ptr<CommandBuffer> commandBuffer, std::shared_ptr<State> state);
  std::shared_ptr<ResourceManager> getResourceManager();
  std::shared_ptr<CameraManager> getCameraManager();
  std::shared_ptr<LightManager> getLightManager();
};