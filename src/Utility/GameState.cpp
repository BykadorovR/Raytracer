#include "Utility/GameState.h"

GameState::GameState(std::shared_ptr<CommandBuffer> commandBuffer, std::shared_ptr<State> state) {
  _resourceManager = std::make_shared<ResourceManager>(state);
#ifdef __ANDROID__
  _resourceManager->setAssetManager(state->getAssetManager());
#endif
  _resourceManager->initialize(commandBuffer);
  _lightManager = std::make_shared<LightManager>(_resourceManager, state);
  _cameraManager = std::make_shared<CameraManager>();
}

std::shared_ptr<ResourceManager> GameState::getResourceManager() { return _resourceManager; }

std::shared_ptr<CameraManager> GameState::getCameraManager() { return _cameraManager; }

std::shared_ptr<LightManager> GameState::getLightManager() { return _lightManager; }