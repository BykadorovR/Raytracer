#pragma once
#include "Utility/EngineState.h"
#include "Utility/GameState.h"
#include "Utility/Settings.h"
#include "Utility/Logger.h"
#include "Utility/GUI.h"
#include "Utility/Timer.h"
#include "Utility/ResourceManager.h"
#include "Utility/Animation.h"
#include "Utility/GameState.h"
#include "Vulkan/Sync.h"
#include "Vulkan/Render.h"
#include "Vulkan/Swapchain.h"
#include "Graphic/Postprocessing.h"
#include "Graphic/Camera.h"
#include "Graphic/LightManager.h"
#include "Graphic/IBL.h"
#include "Graphic/Blur.h"
#include "Primitive/ParticleSystem.h"
#include "Primitive/Terrain.h"
#include "Primitive/Skybox.h"
#include "Primitive/Shape3D.h"
#include "Primitive/Sprite.h"
#include "Primitive/Line.h"
#include "Primitive/Model.h"
#include "Primitive/Equirectangular.h"
#include "BS_thread_pool.hpp"

class Core {
 private:
#ifdef __ANDROID__
  AAssetManager* _assetManager;
  ANativeWindow* _nativeWindow;
#endif
  std::shared_ptr<EngineState> _engineState;
  std::shared_ptr<GameState> _gameState;
  std::shared_ptr<Swapchain> _swapchain;
  std::shared_ptr<ImageView> _depthAttachmentImageView;
  // for compute render pass isn't needed
  std::shared_ptr<RenderPass> _renderPassShadowMap, _renderPassGraphic, _renderPassDebug, _renderPassBlur;
  std::vector<std::shared_ptr<Framebuffer>> _frameBufferGraphic, _frameBufferDebug;
  std::shared_ptr<CommandPool> _commandPoolRender, _commandPoolApplication, _commandPoolInitialize,
      _commandPoolParticleSystem, _commandPoolEquirectangular, _commandPoolPostprocessing, _commandPoolGUI;
  std::vector<std::shared_ptr<CommandBuffer>> _commandBufferRender, _commandBufferApplication, _commandBufferInitialize,
      _commandBufferEquirectangular, _commandBufferParticleSystem, _commandBufferPostprocessing, _commandBufferGUI;

  std::vector<std::shared_ptr<Semaphore>> _semaphoreImageAvailable, _semaphoreRenderFinished;
  std::vector<std::shared_ptr<Semaphore>> _semaphoreParticleSystem, _semaphorePostprocessing, _semaphoreGUI;
  std::vector<std::shared_ptr<Semaphore>> _semaphoreResourcesReady, _semaphoreApplicationReady;
  std::map<int, bool> _waitSemaphoreResourcesReady, _waitSemaphoreApplicationReady;

  std::vector<std::shared_ptr<Fence>> _fenceInFlight;

  std::vector<std::shared_ptr<Texture>> _textureRender, _textureBlurIn, _textureBlurOut;
  std::set<std::shared_ptr<Material>> _materials;
  std::shared_ptr<GUI> _gui;

  std::shared_ptr<Timer> _timer;
  std::shared_ptr<TimerFPS> _timerFPSReal;
  std::shared_ptr<TimerFPS> _timerFPSLimited;

  std::map<AlphaType, std::vector<std::shared_ptr<Drawable>>> _drawables;
  std::map<int, std::vector<std::shared_ptr<Drawable>>> _unusedDrawable;
  std::map<int, std::vector<std::shared_ptr<Shadowable>>> _unusedShadowable;
  std::vector<std::shared_ptr<Shadowable>> _shadowables;
  std::vector<std::shared_ptr<Animation>> _animations;
  std::map<std::shared_ptr<Animation>, std::future<void>> _futureAnimationUpdate;

  std::vector<std::shared_ptr<ParticleSystem>> _particleSystem;
  std::shared_ptr<Postprocessing> _postprocessing;
  std::shared_ptr<Skybox> _skybox = nullptr;
  std::shared_ptr<BlurCompute> _blurCompute;
  std::map<std::shared_ptr<DirectionalShadow>, std::shared_ptr<DirectionalShadowBlur>> _blurGraphicDirectional;
  std::map<std::shared_ptr<PointShadow>, std::shared_ptr<PointShadowBlur>> _blurGraphicPoint;
  std::shared_ptr<BS::thread_pool> _pool;
  std::function<void()> _callbackUpdate;
  std::function<void(int width, int height)> _callbackReset;

  std::vector<std::vector<VkSubmitInfo>> _frameSubmitInfoPreCompute, _frameSubmitInfoPostCompute,
      _frameSubmitInfoGraphic, _frameSubmitInfoDebug;
  std::mutex _frameSubmitMutexGraphic;

  void _drawShadowMapDirectional(int index);
  void _drawShadowMapPoint(int index, int face);
  void _computeParticles();
  void _drawShadowMapDirectionalBlur(std::shared_ptr<DirectionalShadow> directionalShadow);
  void _drawShadowMapPointBlur(std::shared_ptr<PointShadow> pointShadow, int face);
  void _computePostprocessing(int swapchainImageIndex);
  void _debugVisualizations(int swapchainImageIndex);
  void _initializeTextures();
  void _initializeFramebuffer();
  void _renderGraphic();

  VkResult _getImageIndex(uint32_t* imageIndex);
  void _displayFrame(uint32_t* imageIndex);
  void _drawFrame(int imageIndex);
  void _clearUnusedData();
  void _reset();

 public:
  Core(std::shared_ptr<Settings> settings);
#ifdef __ANDROID__
  void setAssetManager(AAssetManager* assetManager);
  void setNativeWindow(ANativeWindow* window);
#endif
  void initialize();
  void draw();
  void registerUpdate(std::function<void()> update);
  void registerReset(std::function<void(int width, int height)> reset);

  void startRecording();
  void endRecording();
  void setCamera(std::shared_ptr<Camera> camera);
  void addDrawable(std::shared_ptr<Drawable> drawable, AlphaType type = AlphaType::TRANSPARENT);
  void addShadowable(std::shared_ptr<Shadowable> shadowable);
  // TODO: everything should be drawable
  void addSkybox(std::shared_ptr<Skybox> skybox);
  void addParticleSystem(std::shared_ptr<ParticleSystem> particleSystem);
  void removeDrawable(std::shared_ptr<Drawable> drawable);
  void removeShadowable(std::shared_ptr<Shadowable> shadowable);

  std::shared_ptr<ImageCPU<uint8_t>> loadImageCPU(std::string path);
  std::shared_ptr<BufferImage> loadImageGPU(std::shared_ptr<ImageCPU<uint8_t>> imageCPU);
  std::shared_ptr<Texture> createTexture(std::string path, VkFormat format, int mipMapLevels);
  std::shared_ptr<Cubemap> createCubemap(std::vector<std::string> paths, VkFormat format, int mipMapLevels);
  std::shared_ptr<ModelGLTF> createModelGLTF(std::string path);
  std::shared_ptr<Animation> createAnimation(std::shared_ptr<ModelGLTF> modelGLTF);
  std::shared_ptr<Equirectangular> createEquirectangular(std::string path);
  std::shared_ptr<MaterialColor> createMaterialColor(MaterialTarget target);
  std::shared_ptr<MaterialPhong> createMaterialPhong(MaterialTarget target);
  std::shared_ptr<MaterialPBR> createMaterialPBR(MaterialTarget target);
  std::shared_ptr<Shape3D> createShape3D(ShapeType shapeType,
                                         std::shared_ptr<Mesh3D> mesh,
                                         VkCullModeFlagBits cullMode = VK_CULL_MODE_BACK_BIT);
  std::shared_ptr<Model3D> createModel3D(std::shared_ptr<ModelGLTF> modelGLTF);
  std::shared_ptr<Sprite> createSprite();
  std::shared_ptr<TerrainGPU> createTerrainInterpolation(std::shared_ptr<ImageCPU<uint8_t>> heightmap);
  std::shared_ptr<TerrainGPU> createTerrainComposition(std::shared_ptr<ImageCPU<uint8_t>> heightmap);
  std::shared_ptr<TerrainCPU> createTerrainCPU(std::vector<float> heights, std::tuple<int, int> resolution);
  std::shared_ptr<TerrainCPU> createTerrainCPU(std::shared_ptr<ImageCPU<uint8_t>> heightmap);
  std::shared_ptr<Line> createLine();
  std::shared_ptr<IBL> createIBL();
  std::shared_ptr<ParticleSystem> createParticleSystem(std::vector<Particle> particles,
                                                       std::shared_ptr<Texture> particleTexture);
  std::shared_ptr<Skybox> createSkybox();
  std::shared_ptr<PointLight> createPointLight();
  std::shared_ptr<DirectionalLight> createDirectionalLight();
  std::shared_ptr<AmbientLight> createAmbientLight();
  std::shared_ptr<PointShadow> createPointShadow(std::shared_ptr<PointLight> pointLight, bool blur = false);
  std::shared_ptr<DirectionalShadow> createDirectionalShadow(std::shared_ptr<DirectionalLight> directionalLight,
                                                             bool blur = true);
  std::shared_ptr<PhysicsManager> createPhysicsManager();

  std::shared_ptr<CommandBuffer> getCommandBufferApplication();
  std::shared_ptr<ResourceManager> getResourceManager();
  const std::vector<std::shared_ptr<Drawable>>& getDrawables(AlphaType type);
  std::vector<std::shared_ptr<PointLight>> getPointLights();
  std::vector<std::shared_ptr<DirectionalLight>> getDirectionalLights();
  std::vector<std::shared_ptr<PointShadow>> getPointShadows();
  std::vector<std::shared_ptr<DirectionalShadow>> getDirectionalShadows();
  std::shared_ptr<Postprocessing> getPostprocessing();
  std::shared_ptr<BlurCompute> getBloomBlur();

  std::shared_ptr<EngineState> getEngineState();
  std::shared_ptr<GameState> getGameState();
  std::shared_ptr<Camera> getCamera();
  std::shared_ptr<GUI> getGUI();
  std::tuple<int, int> getFPS();
};