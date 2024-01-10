#pragma once
#include "State.h"
#include "Settings.h"
#include "Logger.h"
#include "GUI.h"
#include "Postprocessing.h"
#include "Camera.h"
#include "LightManager.h"
#include "Timer.h"
#include "SpriteManager.h"
#include "ModelManager.h"
#include "ParticleSystem.h"
#include "Terrain.h"
#include "Blur.h"
#include "Skybox.h"
#include "BS_thread_pool.hpp"
#include "ResourceManager.h"

class Core {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<ResourceManager> _resourceManager;
  std::shared_ptr<CommandPool> _commandPoolRender, _commandPoolTransfer, _commandPoolParticleSystem,
      _commandPoolEquirectangular, _commandPoolPostprocessing, _commandPoolGUI;
  std::shared_ptr<CommandBuffer> _commandBufferRender, _commandBufferTransfer, _commandBufferEquirectangular,
      _commandBufferParticleSystem, _commandBufferPostprocessing, _commandBufferGUI;

  std::vector<std::shared_ptr<CommandBuffer>> _commandBufferDirectional;
  std::vector<std::shared_ptr<CommandPool>> _commandPoolDirectional;
  std::vector<std::vector<std::shared_ptr<CommandBuffer>>> _commandBufferPoint;
  std::shared_ptr<LoggerGPU> _loggerGPU, _loggerPostprocessing, _loggerParticles, _loggerGUI, _loggerGPUDebug;
  std::vector<std::shared_ptr<LoggerGPU>> _loggerGPUDirectional;
  std::vector<std::vector<std::shared_ptr<LoggerGPU>>> _loggerGPUPoint;
  std::shared_ptr<LoggerCPU> _loggerCPU;

  std::vector<std::shared_ptr<Semaphore>> _semaphoreImageAvailable, _semaphoreRenderFinished;
  std::vector<std::shared_ptr<Semaphore>> _semaphoreParticleSystem, _semaphorePostprocessing, _semaphoreGUI;
  std::vector<std::shared_ptr<Fence>> _fenceInFlight, _fenceParticleSystem;

  std::vector<std::shared_ptr<Texture>> _textureRender, _textureBlurIn, _textureBlurOut;

  std::shared_ptr<GUI> _gui;

  std::shared_ptr<CameraOrtho> _cameraOrtho;
  std::shared_ptr<CameraFly> _cameraFly;
  int _currentFrame;
  std::shared_ptr<Timer> _timer;
  std::shared_ptr<TimerFPS> _timerFPSReal;
  std::shared_ptr<TimerFPS> _timerFPSLimited;

  std::vector<std::shared_ptr<IDrawable>> _drawables;
  std::vector<std::shared_ptr<IShadowable>> _shadowables;
  std::vector<std::shared_ptr<Animation>> _animations;
  std::map<std::shared_ptr<Animation>, std::future<void>> _futureAnimationUpdate;

  std::vector<std::shared_ptr<ParticleSystem>> _particleSystem;
  std::shared_ptr<Postprocessing> _postprocessing;
  std::shared_ptr<LightManager> _lightManager;
  std::shared_ptr<Skybox> _skybox = nullptr;
  std::shared_ptr<Blur> _blur;
  std::shared_ptr<BS::thread_pool> _pool;
  std::function<void()> _update;

  void _directionalLightCalculator(int index);
  void _pointLightCalculator(int index, int face);
  void _computeParticles();
  void _computePostprocessing(int swapchainImageIndex);
  void _debugVisualizations(int swapchainImageIndex);
  void _renderGraphic();

  void _getImageIndex(uint32_t* imageIndex);
  void _displayFrame(uint32_t* imageIndex);
  void _drawFrame(int imageIndex);

 public:
  Core(std::shared_ptr<Settings> settings);
  void addDrawable(std::shared_ptr<IDrawable> drawable);
  void addShadowable(std::shared_ptr<IShadowable> shadowable);
  // TODO: everything should be drawable
  void addParticleSystem(std::shared_ptr<ParticleSystem> particleSystem);
  std::shared_ptr<CommandBuffer> getCommandBufferTransfer();
  std::shared_ptr<LightManager> getLightManager();
  std::shared_ptr<ResourceManager> getResourceManager();
  std::shared_ptr<State> getState();
  void draw();
  void registerUpdate(std::function<void()> update);
};