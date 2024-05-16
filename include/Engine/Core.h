#pragma once
#include "State.h"
#include "Swapchain.h"
#include "Settings.h"
#include "Logger.h"
#include "GUI.h"
#include "Postprocessing.h"
#include "Camera.h"
#include "LightManager.h"
#include "Timer.h"
#include "ParticleSystem.h"
#include "Terrain.h"
#include "Blur.h"
#include "Skybox.h"
#include "BS_thread_pool.hpp"
#include "ResourceManager.h"
#include "Animation.h"
#include "Render.h"

class Core {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Swapchain> _swapchain;
  std::shared_ptr<ImageView> _depthAttachmentImageView;
  // for compute render pass isn't needed
  std::shared_ptr<RenderPass> _renderPassLightDepth, _renderPassGraphic, _renderPassDebug;
  std::vector<std::shared_ptr<Framebuffer>> _frameBufferGraphic, _frameBufferDebug;
  // store for each light, number in flight frame buffers
  std::vector<std::vector<std::shared_ptr<Framebuffer>>> _frameBufferDirectionalLightDepth;
  std::vector<std::vector<std::vector<std::shared_ptr<Framebuffer>>>> _frameBufferPointLightDepth;

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
  std::vector<std::shared_ptr<Fence>> _fenceInFlight;

  std::vector<std::shared_ptr<Texture>> _textureRender, _textureBlurIn, _textureBlurOut;

  std::shared_ptr<GUI> _gui;
  std::shared_ptr<Camera> _camera;

  std::shared_ptr<CameraOrtho> _cameraOrtho;
  std::shared_ptr<Timer> _timer;
  std::shared_ptr<TimerFPS> _timerFPSReal;
  std::shared_ptr<TimerFPS> _timerFPSLimited;

  std::map<AlphaType, std::vector<std::shared_ptr<Drawable>>> _drawables;
  std::vector<std::shared_ptr<Shadowable>> _shadowables;
  std::vector<std::shared_ptr<Animation>> _animations;
  std::map<std::shared_ptr<Animation>, std::future<void>> _futureAnimationUpdate;

  std::vector<std::shared_ptr<ParticleSystem>> _particleSystem;
  std::shared_ptr<Postprocessing> _postprocessing;
  std::shared_ptr<LightManager> _lightManager;
  std::shared_ptr<Skybox> _skybox = nullptr;
  std::shared_ptr<Blur> _blur;
  std::shared_ptr<BS::thread_pool> _pool;
  std::function<void()> _callbackUpdate;
  std::function<void(int width, int height)> _callbackReset;

  std::vector<std::vector<VkSubmitInfo>> _frameSubmitInfoPreCompute, _frameSubmitInfoPostCompute,
      _frameSubmitInfoGraphic, _frameSubmitInfoDebug;
  std::mutex _frameSubmitMutexGraphic;

  void _directionalLightCalculator(int index);
  void _pointLightCalculator(int index, int face);
  void _computeParticles();
  void _computePostprocessing(int swapchainImageIndex);
  void _debugVisualizations(int swapchainImageIndex);
  void _initializeTextures();
  void _renderGraphic();

  VkResult _getImageIndex(uint32_t* imageIndex);
  void _displayFrame(uint32_t* imageIndex);
  void _drawFrame(int imageIndex);
  void _reset();

 public:
  Core(std::shared_ptr<Settings> settings);
  void setCamera(std::shared_ptr<Camera> camera);
  void addDrawable(std::shared_ptr<Drawable> drawable, AlphaType type = AlphaType::TRANSPARENT);
  void addShadowable(std::shared_ptr<Shadowable> shadowable);
  void addSkybox(std::shared_ptr<Skybox> skybox);
  void addAnimation(std::shared_ptr<Animation> animation);

  const std::vector<std::shared_ptr<Drawable>>& getDrawables(AlphaType type);
  void removeDrawable(std::shared_ptr<Drawable> drawable);
  // TODO: everything should be drawable
  void addParticleSystem(std::shared_ptr<ParticleSystem> particleSystem);
  std::shared_ptr<CommandBuffer> getCommandBufferTransfer();
  std::shared_ptr<PointLight> createPointLight(std::tuple<int, int> resolution);
  std::shared_ptr<DirectionalLight> createDirectionalLight(std::tuple<int, int> resolution);
  std::shared_ptr<LightManager> getLightManager();
  std::shared_ptr<ResourceManager> getResourceManager();
  std::shared_ptr<Postprocessing> getPostprocessing();
  std::shared_ptr<Blur> getBlur();
  std::shared_ptr<State> getState();
  std::shared_ptr<GUI> getGUI();
  std::tuple<int, int> getFPS();
  void draw();
  void registerUpdate(std::function<void()> update);
  void registerReset(std::function<void(int width, int height)> reset);
};