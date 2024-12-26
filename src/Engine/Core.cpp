#include "Engine/Core.h"
#include "Primitive/TerrainInterpolation.h"
#include "Primitive/TerrainComposition.h"
#include <typeinfo>

Core::Core(std::shared_ptr<Settings> settings) { _engineState = std::make_shared<EngineState>(settings); }

void Core::_initializeTextures() {
  auto settings = _engineState->getSettings();
  _textureRender.resize(settings->getMaxFramesInFlight());
  _textureBlurIn.resize(settings->getMaxFramesInFlight());
  _textureBlurOut.resize(settings->getMaxFramesInFlight());
  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    auto graphicImage = std::make_shared<Image>(settings->getResolution(), 1, 1, settings->getGraphicColorFormat(),
                                                VK_IMAGE_TILING_OPTIMAL,
                                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _engineState);
    graphicImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                               _commandBufferInitialize);
    auto graphicImageView = std::make_shared<ImageView>(graphicImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                        VK_IMAGE_ASPECT_COLOR_BIT, _engineState);
    _textureRender[i] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, VK_FILTER_LINEAR,
                                                  graphicImageView, _engineState);
    {
      auto blurImage = std::make_shared<Image>(settings->getResolution(), 1, 1, settings->getGraphicColorFormat(),
                                               VK_IMAGE_TILING_OPTIMAL,
                                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _engineState);
      blurImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                              _commandBufferInitialize);
      auto blurImageView = std::make_shared<ImageView>(blurImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                       VK_IMAGE_ASPECT_COLOR_BIT, _engineState);
      _textureBlurIn[i] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, VK_FILTER_LINEAR,
                                                    blurImageView, _engineState);
    }
    {
      auto blurImage = std::make_shared<Image>(settings->getResolution(), 1, 1, settings->getGraphicColorFormat(),
                                               VK_IMAGE_TILING_OPTIMAL,
                                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _engineState);
      blurImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                              _commandBufferInitialize);
      auto blurImageView = std::make_shared<ImageView>(blurImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                       VK_IMAGE_ASPECT_COLOR_BIT, _engineState);
      _textureBlurOut[i] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, VK_FILTER_LINEAR,
                                                     blurImageView, _engineState);
    }
  }

  auto depthAttachment = std::make_shared<Image>(settings->getResolution(), 1, 1, settings->getDepthFormat(),
                                                 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _engineState);
  _depthAttachmentImageView = std::make_shared<ImageView>(depthAttachment, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                          VK_IMAGE_ASPECT_DEPTH_BIT, _engineState);
}

void Core::_initializeFramebuffer() {
  _frameBufferGraphic.resize(_engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _frameBufferGraphic[i] = std::make_shared<Framebuffer>(
        std::vector{_textureRender[i]->getImageView(), _textureBlurIn[i]->getImageView(), _depthAttachmentImageView},
        _textureRender[i]->getImageView()->getImage()->getResolution(), _renderPassGraphic, _engineState->getDevice());
  }

  _frameBufferDebug.resize(_swapchain->getImageViews().size());
  for (int i = 0; i < _frameBufferDebug.size(); i++) {
    _frameBufferDebug[i] = std::make_shared<Framebuffer>(std::vector{_swapchain->getImageViews()[i]},
                                                         _swapchain->getImageViews()[i]->getImage()->getResolution(),
                                                         _renderPassDebug, _engineState->getDevice());
  }
}

#ifdef __ANDROID__
void Core::setAssetManager(AAssetManager* assetManager) { _assetManager = assetManager; }
void Core::setNativeWindow(ANativeWindow* window) { _nativeWindow = window; }
#endif

void Core::initialize() {
  try {
#ifdef __ANDROID__
    _engineState->setNativeWindow(_nativeWindow);
    _engineState->setAssetManager(_assetManager);
#endif
    _engineState->initialize();
    auto settings = _engineState->getSettings();
    _swapchain = std::make_shared<Swapchain>(_engineState);
    _timer = std::make_shared<Timer>();
    _timerFPSReal = std::make_shared<TimerFPS>();
    _timerFPSLimited = std::make_shared<TimerFPS>();
    auto loggerUtils = std::make_shared<LoggerUtils>(_engineState);
    loggerUtils->setName("Queue graphic", VkObjectType::VK_OBJECT_TYPE_QUEUE,
                         _engineState->getDevice()->getQueue(vkb::QueueType::graphics));
    loggerUtils->setName("Queue present", VkObjectType::VK_OBJECT_TYPE_QUEUE,
                         _engineState->getDevice()->getQueue(vkb::QueueType::present));
    loggerUtils->setName("Queue compute", VkObjectType::VK_OBJECT_TYPE_QUEUE,
                         _engineState->getDevice()->getQueue(vkb::QueueType::compute));
    {
      _commandPoolRender = std::make_shared<CommandPool>(vkb::QueueType::graphics, _engineState->getDevice());
      _commandBufferRender = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(), _commandPoolRender,
                                                             _engineState);
      loggerUtils->setName("Command buffer for render graphic", VkObjectType::VK_OBJECT_TYPE_COMMAND_BUFFER,
                           _commandBufferRender->getCommandBuffer());
    }
    {
      _commandPoolApplication = std::make_shared<CommandPool>(vkb::QueueType::graphics, _engineState->getDevice());
      // frameInFlight != 0 can be used in reset
      _commandBufferApplication = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(),
                                                                  _commandPoolApplication, _engineState);
      loggerUtils->setName("Command buffer for appplication", VkObjectType::VK_OBJECT_TYPE_COMMAND_BUFFER,
                           _commandBufferApplication->getCommandBuffer());
    }
    {
      _commandPoolInitialize = std::make_shared<CommandPool>(vkb::QueueType::graphics, _engineState->getDevice());
      _commandBufferInitialize = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(),
                                                                 _commandPoolInitialize, _engineState);
      loggerUtils->setName("Command buffer for initialize", VkObjectType::VK_OBJECT_TYPE_COMMAND_BUFFER,
                           _commandBufferInitialize->getCommandBuffer());
    }
    {
      _commandPoolEquirectangular = std::make_shared<CommandPool>(vkb::QueueType::graphics, _engineState->getDevice());
      _commandBufferEquirectangular = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(),
                                                                      _commandPoolEquirectangular, _engineState);
      loggerUtils->setName("Command buffer for equirectangular", VkObjectType::VK_OBJECT_TYPE_COMMAND_BUFFER,
                           _commandBufferEquirectangular->getCommandBuffer());
    }
    {
      _commandPoolParticleSystem = std::make_shared<CommandPool>(vkb::QueueType::compute, _engineState->getDevice());
      _commandBufferParticleSystem = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(),
                                                                     _commandPoolParticleSystem, _engineState);
      loggerUtils->setName("Command buffer for particle system", VkObjectType::VK_OBJECT_TYPE_COMMAND_BUFFER,
                           _commandBufferParticleSystem->getCommandBuffer());
    }
    {
      _commandPoolPostprocessing = std::make_shared<CommandPool>(vkb::QueueType::compute, _engineState->getDevice());
      _commandBufferPostprocessing = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(),
                                                                     _commandPoolPostprocessing, _engineState);
      loggerUtils->setName("Command buffer for postprocessing", VkObjectType::VK_OBJECT_TYPE_COMMAND_BUFFER,
                           _commandBufferPostprocessing->getCommandBuffer());
    }
    {
      _commandPoolGUI = std::make_shared<CommandPool>(vkb::QueueType::graphics, _engineState->getDevice());
      _commandBufferGUI = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(), _commandPoolGUI,
                                                          _engineState);
      loggerUtils->setName("Command buffer for GUI", VkObjectType::VK_OBJECT_TYPE_COMMAND_BUFFER,
                           _commandBufferGUI->getCommandBuffer());
    }

    _frameSubmitInfoPreCompute.resize(settings->getMaxFramesInFlight());
    _frameSubmitInfoGraphic.resize(settings->getMaxFramesInFlight());
    _frameSubmitInfoPostCompute.resize(settings->getMaxFramesInFlight());
    _frameSubmitInfoDebug.resize(settings->getMaxFramesInFlight());

    _renderPassGraphic = _engineState->getRenderPassManager()->getRenderPass(RenderPassScenario::GRAPHIC);
    _renderPassShadowMap = _engineState->getRenderPassManager()->getRenderPass(RenderPassScenario::SHADOW);
    _renderPassDebug = _engineState->getRenderPassManager()->getRenderPass(RenderPassScenario::GUI);
    _renderPassBlur = _engineState->getRenderPassManager()->getRenderPass(RenderPassScenario::BLUR);

    // start transfer command buffer
    _commandBufferInitialize->beginCommands();

    _logger = std::make_shared<Logger>(_engineState);
    _loggerPostprocessing = std::make_shared<Logger>(_engineState);
    _loggerParticles = std::make_shared<Logger>(_engineState);
    _loggerGUI = std::make_shared<Logger>(_engineState);
    _loggerDebug = std::make_shared<Logger>(_engineState);

    for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
      // graphic-presentation
      _semaphoreImageAvailable.push_back(std::make_shared<Semaphore>(_engineState->getDevice()));
      _semaphoreRenderFinished.push_back(std::make_shared<Semaphore>(_engineState->getDevice()));

      // compute-graphic
      _semaphoreParticleSystem.push_back(std::make_shared<Semaphore>(_engineState->getDevice()));
      _semaphoreGUI.push_back(std::make_shared<Semaphore>(_engineState->getDevice()));

      // postprocessing semaphore
      _semaphorePostprocessing.push_back(std::make_shared<Semaphore>(_engineState->getDevice()));

      // resources submit semaphore
      _semaphoreResourcesReady.push_back(std::make_shared<Semaphore>(_engineState->getDevice()));
      _waitSemaphoreResourcesReady[i] = false;

      // application submit semaphore
      _semaphoreApplicationReady.push_back(std::make_shared<Semaphore>(_engineState->getDevice()));
      _waitSemaphoreApplicationReady[i] = false;
    }

    for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
      _fenceInFlight.push_back(std::make_shared<Fence>(_engineState->getDevice()));
    }

    _initializeTextures();

    _gui = std::make_shared<GUI>(_engineState);
    _gui->initialize(_commandBufferInitialize);

    _blurCompute = std::make_shared<BlurCompute>(_textureBlurIn, _textureBlurOut, _engineState);
    // for postprocessing layout GENERAL is needed
    for (auto& imageView : _swapchain->getImageViews()) imageView->getImage()->overrideLayout(VK_IMAGE_LAYOUT_GENERAL);

    _postprocessing = std::make_shared<Postprocessing>(_textureRender, _textureBlurIn, _swapchain->getImageViews(),
                                                       _engineState);
    // but we expect it to be in VK_IMAGE_LAYOUT_PRESENT_SRC_KHR as start value
    for (auto& imageView : _swapchain->getImageViews())
      imageView->getImage()->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                          VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, _commandBufferInitialize);
    _engineState->getInput()->subscribe(std::dynamic_pointer_cast<InputSubscriberExclusive>(_gui));

    _pool = std::make_shared<BS::thread_pool>(settings->getThreadsInPool());

    _gameState = std::make_shared<GameState>(_commandBufferInitialize, _engineState);

    _commandBufferInitialize->endCommands();

    _initializeFramebuffer();

    {
      std::vector<VkSemaphore> signalSemaphoresInitialize = {
          _semaphoreResourcesReady[_engineState->getFrameInFlight()]->getSemaphore()};
      VkSubmitInfo submitInfo{
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          .commandBufferCount = 1,
          .pCommandBuffers = &_commandBufferInitialize->getCommandBuffer()[_engineState->getFrameInFlight()],
          .signalSemaphoreCount = static_cast<uint32_t>(signalSemaphoresInitialize.size()),
          .pSignalSemaphores = signalSemaphoresInitialize.data()};

      auto queue = _engineState->getDevice()->getQueue(vkb::QueueType::graphics);
      vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);

      _waitSemaphoreResourcesReady[_engineState->getFrameInFlight()] = true;
    }
  } catch (std::exception e) {
    std::cerr << e.what() << std::endl;
  }
}

void Core::_computeParticles() {
  auto frameInFlight = _engineState->getFrameInFlight();

  _commandBufferParticleSystem->beginCommands();

  // any read from SSBO should wait for write to SSBO
  // First dispatch writes to a storage buffer, second dispatch reads from that storage buffer.
  VkMemoryBarrier memoryBarrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT};
  vkCmdPipelineBarrier(_commandBufferParticleSystem->getCommandBuffer()[frameInFlight],
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // srcStageMask
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // dstStageMask
                       0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

  _loggerParticles->begin("Particle system compute " + std::to_string(_timer->getFrameCounter()),
                          _commandBufferParticleSystem);
  for (auto& particleSystem : _particleSystem) {
    particleSystem->drawCompute(_commandBufferParticleSystem);
    particleSystem->updateTimer(_timer->getElapsedCurrent());
  }
  _loggerParticles->end(_commandBufferParticleSystem);
  _commandBufferParticleSystem->endCommands();
}

void Core::_drawShadowMapDirectional(int index) {
  auto frameInFlight = _engineState->getFrameInFlight();
  auto shadow = _gameState->getLightManager()->getDirectionalShadows()[index];

  auto commandBuffer = shadow->getShadowMapCommandBuffer();
  auto loggerGPU = shadow->getShadowMapLogger();

  // record command buffer
  commandBuffer->beginCommands();
  loggerGPU->begin("Directional to depth buffer " + std::to_string(_timer->getFrameCounter()), commandBuffer);
  //
  auto [widthFramebuffer, heightFramebuffer] = shadow->getShadowMapFramebuffer()[frameInFlight]->getResolution();
  VkClearValue clearDepth{.color = {1.f, 1.f, 1.f, 1.f}};
  VkRenderPassBeginInfo renderPassInfo{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                       .renderPass = _renderPassShadowMap->getRenderPass(),
                                       .framebuffer = shadow->getShadowMapFramebuffer()[frameInFlight]->getBuffer(),
                                       .renderArea = {.offset = {0, 0},
                                                      .extent = {.width = static_cast<uint32_t>(widthFramebuffer),
                                                                 .height = static_cast<uint32_t>(heightFramebuffer)}},
                                       .clearValueCount = 1,
                                       .pClearValues = &clearDepth};

  // TODO: only one depth texture?
  vkCmdBeginRenderPass(commandBuffer->getCommandBuffer()[frameInFlight], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  // Set depth bias (aka "Polygon offset")
  // Required to avoid shadow mapping artifacts
  vkCmdSetDepthBias(commandBuffer->getCommandBuffer()[frameInFlight],
                    _engineState->getSettings()->getDepthBiasConstant(), 0.0f,
                    _engineState->getSettings()->getDepthBiasSlope());

  // draw scene here
  auto globalFrame = _timer->getFrameCounter();
  for (auto shadowable : _shadowables) {
    loggerGPU->begin(shadowable->getName() + " to directional depth buffer " + std::to_string(globalFrame),
                     commandBuffer);
    shadowable->drawShadow(LightType::DIRECTIONAL, index, 0, commandBuffer);
    loggerGPU->end(commandBuffer);
  }
  vkCmdEndRenderPass(commandBuffer->getCommandBuffer()[frameInFlight]);
  loggerGPU->end(commandBuffer);

  // record command buffer
  commandBuffer->endCommands();
}

void Core::_drawShadowMapPoint(int index, int face) {
  auto frameInFlight = _engineState->getFrameInFlight();
  auto shadow = _gameState->getLightManager()->getPointShadows()[index];

  auto commandBuffer = shadow->getShadowMapCommandBuffer()[face];
  auto loggerGPU = shadow->getShadowMapLogger()[face];
  // record command buffer
  commandBuffer->beginCommands();
  loggerGPU->begin("Point to depth buffer " + std::to_string(_timer->getFrameCounter()), commandBuffer);
  auto [widthFramebuffer, heightFramebuffer] = shadow->getShadowMapFramebuffer()[frameInFlight][face]->getResolution();
  VkClearValue clearDepth{.color = {1.f, 1.f, 1.f, 1.f}};
  VkRenderPassBeginInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = _renderPassShadowMap->getRenderPass(),
      .framebuffer = shadow->getShadowMapFramebuffer()[frameInFlight][face]->getBuffer(),
      .renderArea = {.offset = {0, 0},
                     .extent = {.width = static_cast<uint32_t>(widthFramebuffer),
                                .height = static_cast<uint32_t>(heightFramebuffer)}},
      .clearValueCount = 1,
      .pClearValues = &clearDepth};

  // TODO: only one depth texture?
  vkCmdBeginRenderPass(commandBuffer->getCommandBuffer()[frameInFlight], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  // Set depth bias (aka "Polygon offset")
  // Required to avoid shadow mapping artifacts
  vkCmdSetDepthBias(commandBuffer->getCommandBuffer()[frameInFlight],
                    _engineState->getSettings()->getDepthBiasConstant(), 0.0f,
                    _engineState->getSettings()->getDepthBiasSlope());

  // draw scene here
  auto globalFrame = _timer->getFrameCounter();
  float aspect = std::get<0>(_engineState->getSettings()->getResolution()) /
                 std::get<1>(_engineState->getSettings()->getResolution());

  // draw scene here
  for (auto shadowable : _shadowables) {
    loggerGPU->begin(shadowable->getName() + " to point depth buffer " + std::to_string(globalFrame), commandBuffer);
    shadowable->drawShadow(LightType::POINT, index, face, commandBuffer);
    loggerGPU->end(commandBuffer);
  }
  vkCmdEndRenderPass(commandBuffer->getCommandBuffer()[frameInFlight]);
  loggerGPU->end(commandBuffer);

  // record command buffer
  commandBuffer->endCommands();
}

void Core::_drawShadowMapPointBlur(std::shared_ptr<PointShadow> pointShadow, int face) {
  auto frameInFlight = _engineState->getFrameInFlight();
  auto commandBufferBlur = _blurGraphicPoint[pointShadow]->getShadowMapBlurCommandBuffer()[face];
  auto loggerGPU = _blurGraphicPoint[pointShadow]->getShadowMapBlurLogger()[face];

  commandBufferBlur->beginCommands();
  loggerGPU->begin("Blur point " + std::to_string(_timer->getFrameCounter()), commandBufferBlur);
  auto [widthFramebuffer, heightFramebuffer] =
      _blurGraphicPoint[pointShadow]->getShadowMapBlurFramebuffer()[0][frameInFlight][face]->getResolution();
  VkClearValue clearDepth{.color = {0.f, 0.f, 0.f, 1.f}};
  VkRenderPassBeginInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = _renderPassBlur->getRenderPass(),
      .framebuffer = _blurGraphicPoint[pointShadow]->getShadowMapBlurFramebuffer()[0][frameInFlight][face]->getBuffer(),
      .renderArea = {.offset = {0, 0},
                     .extent = {.width = static_cast<uint32_t>(widthFramebuffer),
                                .height = static_cast<uint32_t>(heightFramebuffer)}},
      .clearValueCount = 1,
      .pClearValues = &clearDepth};
  vkCmdBeginRenderPass(commandBufferBlur->getCommandBuffer()[frameInFlight], &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
  loggerGPU->begin("Horizontal " + std::to_string(_timer->getFrameCounter()), commandBufferBlur);
  _blurGraphicPoint[pointShadow]->getBlur()[face]->draw(true, commandBufferBlur);
  loggerGPU->end(commandBufferBlur);
  vkCmdEndRenderPass(commandBufferBlur->getCommandBuffer()[frameInFlight]);
  // wait blur image to be ready
  {
    VkImageMemoryBarrier imageMemoryBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                                            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                            .image = _blurGraphicPoint[pointShadow]
                                                         ->getShadowMapBlurCubemapOut()[frameInFlight]
                                                         ->getTextureSeparate()[face][0]
                                                         ->getImageView()
                                                         ->getImage()
                                                         ->getImage(),
                                            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    vkCmdPipelineBarrier(commandBufferBlur->getCommandBuffer()[frameInFlight],
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &imageMemoryBarrier);
  }

  renderPassInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = _renderPassBlur->getRenderPass(),
      .framebuffer = _blurGraphicPoint[pointShadow]->getShadowMapBlurFramebuffer()[1][frameInFlight][face]->getBuffer(),
      .renderArea = {.offset = {0, 0},
                     .extent = {.width = static_cast<uint32_t>(widthFramebuffer),
                                .height = static_cast<uint32_t>(heightFramebuffer)}},
      .clearValueCount = 1,
      .pClearValues = &clearDepth};
  vkCmdBeginRenderPass(commandBufferBlur->getCommandBuffer()[frameInFlight], &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
  loggerGPU->begin("Vertical " + std::to_string(_timer->getFrameCounter()), commandBufferBlur);
  // Vertical blur
  _blurGraphicPoint[pointShadow]->getBlur()[face]->draw(false, commandBufferBlur);
  loggerGPU->end(commandBufferBlur);
  vkCmdEndRenderPass(commandBufferBlur->getCommandBuffer()[frameInFlight]);

  // wait blur image to be ready
  // blurTextureIn stores result after blur (vertical part, out - in)
  {
    auto pointShadows = _gameState->getLightManager()->getPointShadows();
    int indexShadow = std::distance(pointShadows.begin(), find(pointShadows.begin(), pointShadows.end(), pointShadow));
    VkImageMemoryBarrier imageMemoryBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                                            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                            .image = _gameState->getLightManager()
                                                         ->getPointShadows()[indexShadow]
                                                         ->getShadowMapCubemap()[frameInFlight]
                                                         ->getTextureSeparate()[face][0]
                                                         ->getImageView()
                                                         ->getImage()
                                                         ->getImage(),
                                            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    vkCmdPipelineBarrier(commandBufferBlur->getCommandBuffer()[frameInFlight],
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &imageMemoryBarrier);
  }
  loggerGPU->end(commandBufferBlur);
  commandBufferBlur->endCommands();
}

void Core::_drawShadowMapDirectionalBlur(std::shared_ptr<DirectionalShadow> directionalShadow) {
  auto frameInFlight = _engineState->getFrameInFlight();
  auto commandBufferBlur = _blurGraphicDirectional[directionalShadow]->getShadowMapBlurCommandBuffer();
  auto loggerGPU = _blurGraphicDirectional[directionalShadow]->getShadowMapBlurLogger();

  commandBufferBlur->beginCommands();
  loggerGPU->begin("Blur directional " + std::to_string(_timer->getFrameCounter()), commandBufferBlur);
  auto [widthFramebuffer, heightFramebuffer] =
      _blurGraphicDirectional[directionalShadow]->getShadowMapBlurFramebuffer()[0][frameInFlight]->getResolution();
  VkClearValue clearDepth{.color = {0.f, 0.f, 0.f, 1.f}};
  VkRenderPassBeginInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = _renderPassBlur->getRenderPass(),
      .framebuffer =
          _blurGraphicDirectional[directionalShadow]->getShadowMapBlurFramebuffer()[0][frameInFlight]->getBuffer(),
      .renderArea = {.offset = {0, 0},
                     .extent = {.width = static_cast<uint32_t>(widthFramebuffer),
                                .height = static_cast<uint32_t>(heightFramebuffer)}},
      .clearValueCount = 1,
      .pClearValues = &clearDepth};
  vkCmdBeginRenderPass(commandBufferBlur->getCommandBuffer()[frameInFlight], &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
  loggerGPU->begin("Horizontal " + std::to_string(_timer->getFrameCounter()), commandBufferBlur);
  _blurGraphicDirectional[directionalShadow]->getBlur()->draw(true, commandBufferBlur);
  loggerGPU->end(commandBufferBlur);
  vkCmdEndRenderPass(commandBufferBlur->getCommandBuffer()[frameInFlight]);
  // wait blur image to be ready
  {
    VkImageMemoryBarrier imageMemoryBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                                            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                            .image = _blurGraphicDirectional[directionalShadow]
                                                         ->getShadowMapBlurTextureOut()[frameInFlight]
                                                         ->getImageView()
                                                         ->getImage()
                                                         ->getImage(),
                                            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    vkCmdPipelineBarrier(commandBufferBlur->getCommandBuffer()[frameInFlight],
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &imageMemoryBarrier);
  }

  renderPassInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = _renderPassBlur->getRenderPass(),
      .framebuffer =
          _blurGraphicDirectional[directionalShadow]->getShadowMapBlurFramebuffer()[1][frameInFlight]->getBuffer(),
      .renderArea = {.offset = {0, 0},
                     .extent = {.width = static_cast<uint32_t>(widthFramebuffer),
                                .height = static_cast<uint32_t>(heightFramebuffer)}},
      .clearValueCount = 1,
      .pClearValues = &clearDepth};
  vkCmdBeginRenderPass(commandBufferBlur->getCommandBuffer()[frameInFlight], &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
  loggerGPU->begin("Vertical " + std::to_string(_timer->getFrameCounter()), commandBufferBlur);
  // Vertical blur
  _blurGraphicDirectional[directionalShadow]->getBlur()->draw(false, commandBufferBlur);
  loggerGPU->end(commandBufferBlur);
  vkCmdEndRenderPass(commandBufferBlur->getCommandBuffer()[frameInFlight]);

  // wait blur image to be ready
  // blurTextureIn stores result after blur (vertical part, out - in)
  {
    auto directionalShadows = _gameState->getLightManager()->getDirectionalShadows();
    int indexShadow = std::distance(directionalShadows.begin(),
                                    find(directionalShadows.begin(), directionalShadows.end(), directionalShadow));
    VkImageMemoryBarrier imageMemoryBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                                            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                            .image = _gameState->getLightManager()
                                                         ->getDirectionalShadows()[indexShadow]
                                                         ->getShadowMapTexture()[frameInFlight]
                                                         ->getImageView()
                                                         ->getImage()
                                                         ->getImage(),
                                            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    vkCmdPipelineBarrier(commandBufferBlur->getCommandBuffer()[frameInFlight],
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &imageMemoryBarrier);
  }
  loggerGPU->end(commandBufferBlur);
  commandBufferBlur->endCommands();
}

void Core::_computePostprocessing(int swapchainImageIndex) {
  auto frameInFlight = _engineState->getFrameInFlight();

  _commandBufferPostprocessing->beginCommands();
  int bloomPasses = _engineState->getSettings()->getBloomPasses();
  // blur cycle:
  // in - out - horizontal
  // out - in - vertical
  for (int i = 0; i < bloomPasses; i++) {
    _loggerPostprocessing->begin("Blur horizontal compute " + std::to_string(_timer->getFrameCounter()),
                                 _commandBufferPostprocessing);
    _blurCompute->draw(true, _commandBufferPostprocessing);
    _loggerPostprocessing->end(_commandBufferPostprocessing);

    // sync between horizontal and vertical
    // wait dst (textureOut) to be written
    {
      VkImageMemoryBarrier colorBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                                        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                                        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                        .image = _textureBlurOut[frameInFlight]->getImageView()->getImage()->getImage(),
                                        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                             .baseMipLevel = 0,
                                                             .levelCount = 1,
                                                             .baseArrayLayer = 0,
                                                             .layerCount = 1}};
      vkCmdPipelineBarrier(_commandBufferPostprocessing->getCommandBuffer()[frameInFlight],
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // srcStageMask
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // dstStageMask
                           0, 0, nullptr, 0, nullptr,
                           1,             // imageMemoryBarrierCount
                           &colorBarrier  // pImageMemoryBarriers
      );
    }

    _loggerPostprocessing->begin("Blur vertical compute " + std::to_string(_timer->getFrameCounter()),
                                 _commandBufferPostprocessing);
    _blurCompute->draw(false, _commandBufferPostprocessing);
    _loggerPostprocessing->end(_commandBufferPostprocessing);

    // wait blur image to be ready
    // blurTextureIn stores result after blur (vertical part, out - in)
    {
      VkImageMemoryBarrier colorBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                                        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                                        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                        .image = _textureBlurIn[frameInFlight]->getImageView()->getImage()->getImage(),
                                        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                             .baseMipLevel = 0,
                                                             .levelCount = 1,
                                                             .baseArrayLayer = 0,
                                                             .layerCount = 1}};
      vkCmdPipelineBarrier(_commandBufferPostprocessing->getCommandBuffer()[frameInFlight],
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // srcStageMask
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // dstStageMask
                           0, 0, nullptr, 0, nullptr,
                           1,             // imageMemoryBarrierCount
                           &colorBarrier  // pImageMemoryBarriers
      );
    }
  }
  // wait dst image to be ready
  {
    VkImageMemoryBarrier colorBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                      .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                      .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                      .image = _swapchain->getImageViews()[swapchainImageIndex]->getImage()->getImage(),
                                      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                           .baseMipLevel = 0,
                                                           .levelCount = 1,
                                                           .baseArrayLayer = 0,
                                                           .layerCount = 1}};
    vkCmdPipelineBarrier(_commandBufferPostprocessing->getCommandBuffer()[frameInFlight],
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,     // srcStageMask
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // dstStageMask
                         0, 0, nullptr, 0, nullptr,
                         1,             // imageMemoryBarrierCount
                         &colorBarrier  // pImageMemoryBarriers
    );
  }

  _loggerPostprocessing->begin("Postprocessing compute " + std::to_string(_timer->getFrameCounter()),
                               _commandBufferPostprocessing);
  _postprocessing->drawCompute(frameInFlight, swapchainImageIndex, _commandBufferPostprocessing);
  _loggerPostprocessing->end(_commandBufferPostprocessing);
  _commandBufferPostprocessing->endCommands();
}

void Core::_debugVisualizations(int swapchainImageIndex) {
  auto frameInFlight = _engineState->getFrameInFlight();

  _commandBufferGUI->beginCommands();

  auto [widthFramebuffer, heightFramebuffer] = _frameBufferDebug[swapchainImageIndex]->getResolution();
  VkClearValue clearColor{.color = _engineState->getSettings()->getClearColor()};
  VkRenderPassBeginInfo renderPassInfo{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                       .renderPass = _renderPassDebug->getRenderPass(),
                                       .framebuffer = _frameBufferDebug[swapchainImageIndex]->getBuffer(),
                                       .renderArea = {.offset = {0, 0},
                                                      .extent = {.width = static_cast<uint32_t>(widthFramebuffer),
                                                                 .height = static_cast<uint32_t>(heightFramebuffer)}},
                                       .clearValueCount = 1,
                                       .pClearValues = &clearColor};
  // TODO: only one depth texture?
  vkCmdBeginRenderPass(_commandBufferGUI->getCommandBuffer()[frameInFlight], &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
  _loggerDebug->begin("Render GUI " + std::to_string(_timer->getFrameCounter()), _commandBufferGUI);
  _gui->updateBuffers(frameInFlight);
  _gui->drawFrame(frameInFlight, _commandBufferGUI);
  _loggerDebug->end(_commandBufferGUI);
  vkCmdEndRenderPass(_commandBufferGUI->getCommandBuffer()[frameInFlight]);

  _commandBufferGUI->endCommands();
}

void Core::_renderGraphic() {
  auto frameInFlight = _engineState->getFrameInFlight();

  // record command buffer
  _commandBufferRender->beginCommands();
  /////////////////////////////////////////////////////////////////////////////////////////
  // depth to screne barrier
  /////////////////////////////////////////////////////////////////////////////////////////
  // Image memory barrier to make sure that writes are finished before sampling from the texture
  int directionalNum = _gameState->getLightManager()->getDirectionalLights().size();
  int pointNum = _gameState->getLightManager()->getPointLights().size();
  std::vector<VkImageMemoryBarrier> imageMemoryBarrier;
  for (int i = 0; i < directionalNum; i++) {
    if (_gameState->getLightManager()->getDirectionalShadows()[i]) {
      VkImageMemoryBarrier barrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                   .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                   .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                   // We won't be changing the layout of the image
                                   .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                                   .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                   .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                   .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                   .image = _gameState->getLightManager()
                                                ->getDirectionalShadows()[i]
                                                ->getShadowMapTexture()[frameInFlight]
                                                ->getImageView()
                                                ->getImage()
                                                ->getImage(),
                                   .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

      imageMemoryBarrier.push_back(barrier);
    }
  }

  for (int i = 0; i < pointNum; i++) {
    if (_gameState->getLightManager()->getPointShadows()[i]) {
      VkImageMemoryBarrier barrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                   .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                   .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                   .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                                   .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                   .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                   .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                   .image = _gameState->getLightManager()
                                                ->getPointShadows()[i]
                                                ->getShadowMapCubemap()[frameInFlight]
                                                ->getTexture()
                                                ->getImageView()
                                                ->getImage()
                                                ->getImage(),
                                   .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

      imageMemoryBarrier.push_back(barrier);
    }
  }
  vkCmdPipelineBarrier(_commandBufferRender->getCommandBuffer()[frameInFlight],
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                       nullptr, 0, nullptr, imageMemoryBarrier.size(), imageMemoryBarrier.data());

  /////////////////////////////////////////////////////////////////////////////////////////
  // render graphic
  /////////////////////////////////////////////////////////////////////////////////////////
  auto [widthFramebuffer, heightFramebuffer] = _frameBufferGraphic[frameInFlight]->getResolution();
  std::vector<VkClearValue> clearColor{{.color = _engineState->getSettings()->getClearColor()},
                                       {.color = _engineState->getSettings()->getClearColor()},
                                       {.color = {1.0f, 0}}};
  VkRenderPassBeginInfo renderPassInfo{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                       .renderPass = _renderPassGraphic->getRenderPass(),
                                       .framebuffer = _frameBufferGraphic[frameInFlight]->getBuffer(),
                                       .renderArea = {.offset = {0, 0},
                                                      .extent = {.width = static_cast<uint32_t>(widthFramebuffer),
                                                                 .height = static_cast<uint32_t>(heightFramebuffer)}},
                                       .clearValueCount = 3,
                                       .pClearValues = clearColor.data()};

  auto globalFrame = _timer->getFrameCounter();
  // TODO: only one depth texture?
  vkCmdBeginRenderPass(_commandBufferRender->getCommandBuffer()[frameInFlight], &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  _logger->begin("Render light " + std::to_string(globalFrame), _commandBufferRender);
  _gameState->getLightManager()->draw(frameInFlight);
  _logger->end(_commandBufferRender);

  // draw scene here
  for (auto& animation : _animations) {
    _logger->begin("Update animation buffers " + std::to_string(globalFrame));
    if (_futureAnimationUpdate[animation].valid()) {
      _futureAnimationUpdate[animation].get();
    }
    animation->updateBuffers(_engineState->getFrameInFlight());
    _logger->end();
  }

  // should be draw first
  if (_skybox) {
    _logger->begin("Render skybox " + std::to_string(globalFrame), _commandBufferRender);
    _skybox->draw(_commandBufferRender);
    _logger->end(_commandBufferRender);
  }

  for (auto& drawable : _drawables[AlphaType::OPAQUE]) {
    // TODO: add getName() to drawable?
    std::string drawableName = typeid(drawable.get()).name();
    _logger->begin("Render " + drawable->getName() + " " + std::to_string(globalFrame), _commandBufferRender);
    drawable->draw(_commandBufferRender);
    _logger->end(_commandBufferRender);
  }

  std::sort(_drawables[AlphaType::TRANSPARENT].begin(), _drawables[AlphaType::TRANSPARENT].end(),
            [camera = _gameState->getCameraManager()->getCurrentCamera()](std::shared_ptr<Drawable> left,
                                                                          std::shared_ptr<Drawable> right) {
              return glm::distance(glm::vec3(left->getModel()[3]), camera->getEye()) >
                     glm::distance(glm::vec3(right->getModel()[3]), camera->getEye());
            });
  for (auto& drawable : _drawables[AlphaType::TRANSPARENT]) {
    _logger->begin("Render " + drawable->getName() + " " + std::to_string(globalFrame), _commandBufferRender);
    drawable->draw(_commandBufferRender);
    _logger->end(_commandBufferRender);
  }

  // submit model3D update
  for (auto& animation : _animations) {
    _futureAnimationUpdate[animation] = _pool->submit([&, frame = globalFrame]() {
      _logger->begin("Calculate animation joints " + std::to_string(frame));
      // we want update model for next frame, current frame we can't touch and update because it will be used on GPU
      animation->calculateJoints(_timer->getElapsedCurrent());
      _logger->end();
    });
  }

  vkCmdEndRenderPass(_commandBufferRender->getCommandBuffer()[frameInFlight]);
  _commandBufferRender->endCommands();
}

VkResult Core::_getImageIndex(uint32_t* imageIndex) {
  auto frameInFlight = _engineState->getFrameInFlight();
  std::vector<VkFence> waitFences = {_fenceInFlight[frameInFlight]->getFence()};
  auto result = vkWaitForFences(_engineState->getDevice()->getLogicalDevice(), waitFences.size(), waitFences.data(),
                                VK_TRUE, UINT64_MAX);
  if (result != VK_SUCCESS) throw std::runtime_error("Can't wait for fence");

  _frameSubmitInfoPreCompute[frameInFlight].clear();
  _frameSubmitInfoGraphic[frameInFlight].clear();
  _frameSubmitInfoPostCompute[frameInFlight].clear();
  _frameSubmitInfoDebug[frameInFlight].clear();
  // RETURNS ONLY INDEX, NOT IMAGE
  // semaphore to signal, once image is available
  result = vkAcquireNextImageKHR(_engineState->getDevice()->getLogicalDevice(), _swapchain->getSwapchain(), UINT64_MAX,
                                 _semaphoreImageAvailable[frameInFlight]->getSemaphore(), VK_NULL_HANDLE, imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    _reset();
    return result;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  // Only reset the fence if we are submitting work
  result = vkResetFences(_engineState->getDevice()->getLogicalDevice(), waitFences.size(), waitFences.data());
  if (result != VK_SUCCESS) throw std::runtime_error("Can't reset fence");

  return result;
}

void Core::_reset() {
  _swapchain->reset();
  _engineState->getSettings()->setResolution(
      {_swapchain->getSwapchain().extent.width, _swapchain->getSwapchain().extent.height});
  _textureRender.clear();
  _textureBlurIn.clear();
  _textureBlurOut.clear();

  _commandBufferInitialize->beginCommands();

  _initializeTextures();
  for (auto& imageView : _swapchain->getImageViews()) imageView->getImage()->overrideLayout(VK_IMAGE_LAYOUT_GENERAL);
  _postprocessing->reset(_textureRender, _textureBlurIn, _swapchain->getImageViews());
  for (auto& imageView : _swapchain->getImageViews())
    imageView->getImage()->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                        VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, _commandBufferInitialize);
  _gui->reset();
  _blurCompute->reset(_textureBlurIn, _textureBlurOut);

  _commandBufferInitialize->endCommands();

  _initializeFramebuffer();
  {
    std::vector<VkSemaphore> signalSemaphoresInitialize = {
        _semaphoreResourcesReady[_engineState->getFrameInFlight()]->getSemaphore()};
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &_commandBufferInitialize->getCommandBuffer()[_engineState->getFrameInFlight()],
        .signalSemaphoreCount = static_cast<uint32_t>(signalSemaphoresInitialize.size()),
        .pSignalSemaphores = signalSemaphoresInitialize.data()};

    auto queue = _engineState->getDevice()->getQueue(vkb::QueueType::graphics);
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    _waitSemaphoreResourcesReady[_engineState->getFrameInFlight()] = true;
  }

  _callbackReset(_swapchain->getSwapchain().extent.width, _swapchain->getSwapchain().extent.height);
}

void Core::_clearUnusedData() {
  int currentFrame = _engineState->getFrameInFlight();
  _unusedShadowable[currentFrame].clear();
  _unusedDrawable[currentFrame].clear();
}

void Core::_drawFrame(int imageIndex) {
  auto frameInFlight = _engineState->getFrameInFlight();
  // submit compute particles
  auto particlesFuture = _pool->submit(std::bind(&Core::_computeParticles, this));

  _gameState->getCameraManager()->update();

  // first update materials
  for (auto& e : _materials) {
    e->update(frameInFlight);
  }

  /////////////////////////////////////////////////////////////////////////////////////////
  // render to depth buffer
  /////////////////////////////////////////////////////////////////////////////////////////
  std::vector<std::future<void>> shadowFutures;
  std::vector<std::future<void>> shadowBlurFutures;
  {
    auto shadows = _gameState->getLightManager()->getDirectionalShadows();
    for (int i = 0; i < shadows.size(); i++) {
      if (shadows[i]) {
        shadowFutures.push_back(_pool->submit(std::bind(&Core::_drawShadowMapDirectional, this, i)));
        if (_blurGraphicDirectional.find(shadows[i]) != _blurGraphicDirectional.end())
          shadowBlurFutures.push_back(_pool->submit(std::bind(&Core::_drawShadowMapDirectionalBlur, this, shadows[i])));
      }
    }
  }
  {
    auto shadows = _gameState->getLightManager()->getPointShadows();
    for (int i = 0; i < shadows.size(); i++) {
      if (shadows[i]) {
        for (int j = 0; j < 6; j++) {
          shadowFutures.push_back(_pool->submit(std::bind(&Core::_drawShadowMapPoint, this, i, j)));
          if (_blurGraphicPoint.find(shadows[i]) != _blurGraphicPoint.end())
            shadowBlurFutures.push_back(_pool->submit(std::bind(&Core::_drawShadowMapPointBlur, this, shadows[i], j)));
        }
      }
    }
  }

  auto postprocessingFuture = _pool->submit(std::bind(&Core::_computePostprocessing, this, imageIndex));
  auto debugVisualizationFuture = _pool->submit(std::bind(&Core::_debugVisualizations, this, imageIndex));

  /////////////////////////////////////////////////////////////////////////////////////////////////
  // Render graphics
  /////////////////////////////////////////////////////////////////////////////////////////////////
  _renderGraphic();

  // wait for shadow to complete before render
  for (auto& shadowFuture : shadowFutures) {
    if (shadowFuture.valid()) {
      shadowFuture.get();
    }
  }

  for (auto& shadowBlur : shadowBlurFutures) {
    if (shadowBlur.valid()) {
      shadowBlur.get();
    }
  }

  // wait for particles to complete before render
  if (particlesFuture.valid()) particlesFuture.get();
  // submit particles
  VkSubmitInfo submitInfoCompute{.commandBufferCount = 1,
                                 .pCommandBuffers = &_commandBufferParticleSystem->getCommandBuffer()[frameInFlight],
                                 .signalSemaphoreCount = 1,
                                 .pSignalSemaphores = &_semaphoreParticleSystem[frameInFlight]->getSemaphore()};
  submitInfoCompute.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  std::vector<VkPipelineStageFlags> waitStages;
  std::vector<VkSemaphore> semaphoresWaitParticles;
  if (_waitSemaphoreResourcesReady[frameInFlight]) {
    waitStages.push_back(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    semaphoresWaitParticles.push_back(_semaphoreResourcesReady[frameInFlight]->getSemaphore());
    _waitSemaphoreResourcesReady[_engineState->getFrameInFlight()] = false;
  }
  if (_waitSemaphoreApplicationReady[frameInFlight]) {
    waitStages.push_back(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    semaphoresWaitParticles.push_back(_semaphoreApplicationReady[frameInFlight]->getSemaphore());
    _waitSemaphoreApplicationReady[_engineState->getFrameInFlight()] = false;
  }
  if (semaphoresWaitParticles.size() > 0) {
    submitInfoCompute.waitSemaphoreCount = semaphoresWaitParticles.size();
    submitInfoCompute.pWaitSemaphores = semaphoresWaitParticles.data();
    submitInfoCompute.pWaitDstStageMask = waitStages.data();
  }

  // end command buffer
  _frameSubmitInfoPreCompute[frameInFlight].push_back(submitInfoCompute);

  std::vector<VkCommandBuffer> shadowAndGraphicBuffers;
  {
    // binary wait structure
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT};

    VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                            .waitSemaphoreCount = 1,
                            .pWaitSemaphores = &_semaphoreParticleSystem[frameInFlight]->getSemaphore(),
                            .pWaitDstStageMask = waitStages};
    {
      auto shadows = _gameState->getLightManager()->getDirectionalShadows();
      for (int i = 0; i < shadows.size(); i++) {
        if (shadows[i]) {
          shadowAndGraphicBuffers.push_back(_gameState->getLightManager()
                                                ->getDirectionalShadows()[i]
                                                ->getShadowMapCommandBuffer()
                                                ->getCommandBuffer()[frameInFlight]);
          if (_blurGraphicDirectional.find(shadows[i]) != _blurGraphicDirectional.end())
            shadowAndGraphicBuffers.push_back(_blurGraphicDirectional[shadows[i]]
                                                  ->getShadowMapBlurCommandBuffer()
                                                  ->getCommandBuffer()[frameInFlight]);
        }
      }
    }
    {
      auto shadows = _gameState->getLightManager()->getPointShadows();
      for (int i = 0; i < shadows.size(); i++) {
        if (shadows[i]) {
          auto shadowMap = shadows[i]->getShadowMapCommandBuffer();
          for (int j = 0; j < shadowMap.size(); j++) {
            shadowAndGraphicBuffers.push_back(shadowMap[j]->getCommandBuffer()[frameInFlight]);
            if (_blurGraphicPoint.find(shadows[i]) != _blurGraphicPoint.end())
              shadowAndGraphicBuffers.push_back(
                  _blurGraphicPoint[shadows[i]]->getShadowMapBlurCommandBuffer()[j]->getCommandBuffer()[frameInFlight]);
          }
        }
      }
    }
    shadowAndGraphicBuffers.push_back(_commandBufferRender->getCommandBuffer()[frameInFlight]);
    submitInfo.commandBufferCount = shadowAndGraphicBuffers.size();
    submitInfo.pCommandBuffers = shadowAndGraphicBuffers.data();
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &_semaphorePostprocessing[frameInFlight]->getSemaphore();

    std::unique_lock<std::mutex> lock(_frameSubmitMutexGraphic);
    _frameSubmitInfoGraphic[frameInFlight].push_back(submitInfo);
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Render compute postprocessing
  //////////////////////////////////////////////////////////////////////////////////////////////////
  std::vector<VkSemaphore> waitSemaphoresPostprocessing = {_semaphoreImageAvailable[frameInFlight]->getSemaphore(),
                                                           _semaphorePostprocessing[frameInFlight]->getSemaphore()};
  {
    if (postprocessingFuture.valid()) postprocessingFuture.get();

    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
    VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                            .waitSemaphoreCount = static_cast<uint32_t>(waitSemaphoresPostprocessing.size()),
                            .pWaitSemaphores = waitSemaphoresPostprocessing.data(),
                            .pWaitDstStageMask = waitStages,
                            .commandBufferCount = 1,
                            .pCommandBuffers = &_commandBufferPostprocessing->getCommandBuffer()[frameInFlight],
                            .signalSemaphoreCount = 1,
                            .pSignalSemaphores = &_semaphoreGUI[frameInFlight]->getSemaphore()};

    _frameSubmitInfoPostCompute[frameInFlight].push_back(submitInfo);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Render debug visualization
  //////////////////////////////////////////////////////////////////////////////////////////////////
  {
    if (debugVisualizationFuture.valid()) debugVisualizationFuture.get();

    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
    VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                            .waitSemaphoreCount = 1,
                            .pWaitSemaphores = &_semaphoreGUI[frameInFlight]->getSemaphore(),
                            .pWaitDstStageMask = waitStages,
                            .commandBufferCount = 1,
                            .pCommandBuffers = &_commandBufferGUI->getCommandBuffer()[frameInFlight],
                            .signalSemaphoreCount = 1,
                            .pSignalSemaphores = &_semaphoreRenderFinished[frameInFlight]->getSemaphore()};

    _frameSubmitInfoDebug[frameInFlight].push_back(submitInfo);
  }

  // because of binary semaphores, we can't submit task with semaphore wait BEFORE task with semaphore signal.
  // that's why we have to split submit pipeline.
  vkQueueSubmit(_engineState->getDevice()->getQueue(vkb::QueueType::compute),
                _frameSubmitInfoPreCompute[frameInFlight].size(), _frameSubmitInfoPreCompute[frameInFlight].data(),
                VK_NULL_HANDLE);
  vkQueueSubmit(_engineState->getDevice()->getQueue(vkb::QueueType::graphics),
                _frameSubmitInfoGraphic[frameInFlight].size(), _frameSubmitInfoGraphic[frameInFlight].data(),
                VK_NULL_HANDLE);
  vkQueueSubmit(_engineState->getDevice()->getQueue(vkb::QueueType::compute),
                _frameSubmitInfoPostCompute[frameInFlight].size(), _frameSubmitInfoPostCompute[frameInFlight].data(),
                VK_NULL_HANDLE);
  // latest graphic job should signal fence about completion, otherwise render signal that it's done, but debug still in
  // progress (for 0 frame) and we submit new frame with 0 index
  vkQueueSubmit(_engineState->getDevice()->getQueue(vkb::QueueType::graphics),
                _frameSubmitInfoDebug[frameInFlight].size(), _frameSubmitInfoDebug[frameInFlight].data(),
                _fenceInFlight[frameInFlight]->getFence());
}

void Core::_displayFrame(uint32_t* imageIndex) {
  auto frameInFlight = _engineState->getFrameInFlight();

  std::vector<VkSemaphore> waitSemaphoresPresent = {_semaphoreRenderFinished[frameInFlight]->getSemaphore()};
  VkSwapchainKHR swapChains[] = {_swapchain->getSwapchain()};
  VkPresentInfoKHR presentInfo{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                               .waitSemaphoreCount = static_cast<uint32_t>(waitSemaphoresPresent.size()),
                               .pWaitSemaphores = waitSemaphoresPresent.data(),
                               .swapchainCount = 1,
                               .pSwapchains = swapChains,
                               .pImageIndices = imageIndex};

  // TODO: change to own present queue
  auto result = vkQueuePresentKHR(_engineState->getDevice()->getQueue(vkb::QueueType::present), &presentInfo);

  // getResized() can be valid only here, we can get inconsistencies in semaphores if VK_ERROR_OUT_OF_DATE_KHR is not
  // reported by Vulkan
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _engineState->getWindow()->getResized()) {
    _engineState->getWindow()->setResized(false);
    _reset();
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }
}

void Core::draw() {
  try {
#ifdef __ANDROID__
    {
#else
    while (!glfwWindowShouldClose((GLFWwindow*)(_engineState->getWindow()->getWindow()))) {
      glfwPollEvents();
#endif
      _timer->tick();
      _timerFPSReal->tick();
      _timerFPSLimited->tick();
      _engineState->setFrameInFlight(_timer->getFrameCounter() % _engineState->getSettings()->getMaxFramesInFlight());

      // business/application update loop callback
      uint32_t imageIndex;
      while (_getImageIndex(&imageIndex) != VK_SUCCESS)
        ;
      // clear removed entities: drawables and shadowables
      _clearUnusedData();
      // application update, can be anything
      _callbackUpdate();
      // render scene
      _drawFrame(imageIndex);
      _timerFPSReal->tock();
      // if GPU frames are limited by driver it will happen during display
      _displayFrame(&imageIndex);

      _timer->sleep(_engineState->getSettings()->getDesiredFPS());
      _timer->tock();
      _timerFPSLimited->tock();
    }
#ifndef __ANDROID__
    vkDeviceWaitIdle(_engineState->getDevice()->getLogicalDevice());
#endif
  } catch (std::exception e) {
    std::cerr << e.what() << std::endl;
  }
}

void Core::registerUpdate(std::function<void()> update) { _callbackUpdate = update; }

void Core::registerReset(std::function<void(int width, int height)> reset) { _callbackReset = reset; }

void Core::startRecording() { _commandBufferApplication->beginCommands(); }

void Core::endRecording() {
  _commandBufferApplication->endCommands();
  {
    std::vector<VkSemaphore> signalSemaphores = {
        _semaphoreApplicationReady[_engineState->getFrameInFlight()]->getSemaphore()};
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &_commandBufferApplication->getCommandBuffer()[_engineState->getFrameInFlight()],
        .signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size()),
        .pSignalSemaphores = signalSemaphores.data()};

    auto queue = _engineState->getDevice()->getQueue(vkb::QueueType::graphics);
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    _waitSemaphoreApplicationReady[_engineState->getFrameInFlight()] = true;
  }
}

void Core::setCamera(std::shared_ptr<Camera> camera) { _gameState->getCameraManager()->setCurrentCamera(camera); }

void Core::addDrawable(std::shared_ptr<Drawable> drawable, AlphaType type) {
  if (drawable == nullptr) return;

  auto position = std::find(_drawables[type].begin(), _drawables[type].end(), drawable);
  // add only if doesn't exist already
  if (position == _drawables[type].end()) _drawables[type].push_back(drawable);
}

void Core::addShadowable(std::shared_ptr<Shadowable> shadowable) { _shadowables.push_back(shadowable); }

void Core::addSkybox(std::shared_ptr<Skybox> skybox) { _skybox = skybox; }

void Core::addParticleSystem(std::shared_ptr<ParticleSystem> particleSystem) {
  _particleSystem.push_back(particleSystem);
  addDrawable(particleSystem);
}

void Core::removeDrawable(std::shared_ptr<Drawable> drawable) {
  if (drawable == nullptr) return;

  for (auto& [_, drawableVector] : _drawables) {
    auto position = std::find(drawableVector.begin(), drawableVector.end(), drawable);
    // we can remove this object only after current frame on GPU ends processing
    if (position != drawableVector.end()) {
      _unusedDrawable[(_engineState->getFrameInFlight() + 1) % _engineState->getSettings()->getMaxFramesInFlight()]
          .push_back(*position);
      drawableVector.erase(position);
      break;
    }
  }
}

void Core::removeShadowable(std::shared_ptr<Shadowable> shadowable) {
  if (shadowable == nullptr) return;

  auto position = std::find(_shadowables.begin(), _shadowables.end(), shadowable);
  // we can remove this object only after current frame on GPU ends processing
  if (position != _shadowables.end()) {
    _unusedShadowable[(_engineState->getFrameInFlight() + 1) % _engineState->getSettings()->getMaxFramesInFlight()]
        .push_back(*position);
    _shadowables.erase(position);
  }
}

std::shared_ptr<ImageCPU<uint8_t>> Core::loadImageCPU(std::string path) {
  return _gameState->getResourceManager()->loadImageCPU<uint8_t>(path);
}

std::shared_ptr<BufferImage> Core::loadImageGPU(std::shared_ptr<ImageCPU<uint8_t>> imageCPU) {
  return _gameState->getResourceManager()->loadImageGPU<uint8_t>({imageCPU});
}

std::shared_ptr<Texture> Core::createTexture(std::string path, VkFormat format, int mipMapLevels) {
  auto imageCPU = loadImageCPU(path);
  auto texture = std::make_shared<Texture>(loadImageGPU(imageCPU), format, VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels,
                                           VK_FILTER_LINEAR, _commandBufferApplication, _engineState);
  return texture;
}

std::shared_ptr<Cubemap> Core::createCubemap(std::vector<std::string> paths, VkFormat format, int mipMapLevels) {
  std::vector<std::shared_ptr<ImageCPU<uint8_t>>> images;
  for (auto path : paths) {
    images.push_back(loadImageCPU(path));
  }
  return std::make_shared<Cubemap>(
      _gameState->getResourceManager()->loadImageGPU<uint8_t>(images), format, mipMapLevels, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FILTER_LINEAR,
      _commandBufferApplication, _engineState);
}

std::shared_ptr<ModelGLTF> Core::createModelGLTF(std::string path) {
  auto model = _gameState->getResourceManager()->loadModel(path, _commandBufferApplication);
  _materials.insert(model->getMaterialsColor().begin(), model->getMaterialsColor().end());
  _materials.insert(model->getMaterialsPhong().begin(), model->getMaterialsPhong().end());
  _materials.insert(model->getMaterialsPBR().begin(), model->getMaterialsPBR().end());
  return model;
}

std::shared_ptr<Animation> Core::createAnimation(std::shared_ptr<ModelGLTF> modelGLTF) {
  auto animation = std::make_shared<Animation>(modelGLTF->getNodes(), modelGLTF->getSkins(), modelGLTF->getAnimations(),
                                               _engineState);
  _animations.push_back(animation);
  return animation;
}

std::shared_ptr<Equirectangular> Core::createEquirectangular(std::string path) {
  return std::make_shared<Equirectangular>(_gameState->getResourceManager()->loadImageCPU<float>(path),
                                           _commandBufferApplication, _engineState);
}

std::shared_ptr<MaterialColor> Core::createMaterialColor(MaterialTarget target) {
  auto material = std::make_shared<MaterialColor>(target, _commandBufferApplication, _engineState);
  _materials.insert(material);
  return material;
}

std::shared_ptr<MaterialPhong> Core::createMaterialPhong(MaterialTarget target) {
  auto material = std::make_shared<MaterialPhong>(target, _commandBufferApplication, _engineState);
  _materials.insert(material);
  return material;
}

std::shared_ptr<MaterialPBR> Core::createMaterialPBR(MaterialTarget target) {
  auto material = std::make_shared<MaterialPBR>(target, _commandBufferApplication, _engineState);
  _materials.insert(material);
  return material;
}

std::shared_ptr<Shape3D> Core::createShape3D(ShapeType shapeType, VkCullModeFlagBits cullMode) {
  std::shared_ptr<MeshStatic3D> mesh;
  switch (shapeType) {
    case ShapeType::CUBE:
      mesh = std::make_shared<MeshCube>(_commandBufferApplication, _engineState);
      break;
    case ShapeType::SPHERE:
      mesh = std::make_shared<MeshSphere>(_commandBufferApplication, _engineState);
      break;
    case ShapeType::CAPSULE:
      mesh = std::make_shared<MeshCapsule>(1, 1, _commandBufferApplication, _engineState);
  }
  return std::make_shared<Shape3D>(shapeType, mesh, cullMode, _commandBufferApplication, _gameState, _engineState);
}

std::shared_ptr<Shape3D> Core::createCapsule(float height, float radius, VkCullModeFlagBits cullMode) {
  auto mesh = std::make_shared<MeshCapsule>(height, radius, _commandBufferApplication, _engineState);
  return std::make_shared<Shape3D>(ShapeType::CAPSULE, mesh, cullMode, _commandBufferApplication, _gameState,
                                   _engineState);
}

std::shared_ptr<Model3D> Core::createModel3D(std::shared_ptr<ModelGLTF> modelGLTF) {
  return std::make_shared<Model3D>(modelGLTF->getNodes(), modelGLTF->getMeshes(), _commandBufferApplication, _gameState,
                                   _engineState);
}

std::shared_ptr<Sprite> Core::createSprite() {
  return std::make_shared<Sprite>(_commandBufferApplication, _gameState, _engineState);
}

std::shared_ptr<TerrainGPU> Core::createTerrainInterpolation(std::shared_ptr<ImageCPU<uint8_t>> heightmap) {
  return std::make_shared<TerrainInterpolation>(heightmap, _gameState, _engineState);
}

std::shared_ptr<TerrainGPU> Core::createTerrainComposition(std::shared_ptr<ImageCPU<uint8_t>> heightmap) {
  return std::make_shared<TerrainComposition>(heightmap, _gameState, _engineState);
}

std::shared_ptr<TerrainCPU> Core::createTerrainCPU(std::shared_ptr<ImageCPU<uint8_t>> heightmap) {
  return std::make_shared<TerrainCPU>(heightmap, _gameState, _engineState);
}

std::shared_ptr<TerrainCPU> Core::createTerrainCPU(std::vector<float> heights, std::tuple<int, int> resolution) {
  return std::make_shared<TerrainCPU>(heights, resolution, _gameState, _engineState);
}

std::shared_ptr<Line> Core::createLine() {
  return std::make_shared<Line>(_commandBufferApplication, _gameState, _engineState);
}

std::shared_ptr<IBL> Core::createIBL() {
  return std::make_shared<IBL>(_commandBufferApplication, _gameState, _engineState);
}

std::shared_ptr<ParticleSystem> Core::createParticleSystem(std::vector<Particle> particles,
                                                           std::shared_ptr<Texture> particleTexture) {
  return std::make_shared<ParticleSystem>(particles, particleTexture, _commandBufferApplication, _gameState,
                                          _engineState);
}

std::shared_ptr<Skybox> Core::createSkybox() {
  return std::make_shared<Skybox>(_commandBufferApplication, _gameState, _engineState);
}

std::shared_ptr<PointShadow> Core::createPointShadow(std::shared_ptr<PointLight> pointLight, bool blur) {
  auto shadow = _gameState->getLightManager()->createPointShadow(pointLight, _renderPassShadowMap,
                                                                 _commandBufferApplication);

  if (blur) {
    _blurGraphicPoint[shadow] = std::make_shared<PointShadowBlur>(
        shadow->getShadowMapCubemap(), _commandBufferApplication, _renderPassShadowMap, _engineState);
  }

  return shadow;
}

std::shared_ptr<DirectionalShadow> Core::createDirectionalShadow(std::shared_ptr<DirectionalLight> directionalLight,
                                                                 bool blur) {
  auto shadow = _gameState->getLightManager()->createDirectionalShadow(directionalLight, _renderPassShadowMap,
                                                                       _commandBufferApplication);

  if (blur) {
    _blurGraphicDirectional[shadow] = std::make_shared<DirectionalShadowBlur>(
        shadow->getShadowMapTexture(), _commandBufferApplication, _renderPassShadowMap, _engineState);
  }

  return shadow;
}

std::shared_ptr<PointLight> Core::createPointLight() {
  auto light = _gameState->getLightManager()->createPointLight();
  return light;
}

std::shared_ptr<DirectionalLight> Core::createDirectionalLight() {
  auto light = _gameState->getLightManager()->createDirectionalLight();
  return light;
}

std::shared_ptr<AmbientLight> Core::createAmbientLight() { return _gameState->getLightManager()->createAmbientLight(); }

std::shared_ptr<CommandBuffer> Core::getCommandBufferApplication() { return _commandBufferApplication; }

std::shared_ptr<ResourceManager> Core::getResourceManager() { return _gameState->getResourceManager(); }

const std::vector<std::shared_ptr<Drawable>>& Core::getDrawables(AlphaType type) { return _drawables[type]; }

std::vector<std::shared_ptr<PointLight>> Core::getPointLights() {
  return _gameState->getLightManager()->getPointLights();
}

std::vector<std::shared_ptr<DirectionalLight>> Core::getDirectionalLights() {
  return _gameState->getLightManager()->getDirectionalLights();
}

std::vector<std::shared_ptr<PointShadow>> Core::getPointShadows() {
  return _gameState->getLightManager()->getPointShadows();
}

std::vector<std::shared_ptr<DirectionalShadow>> Core::getDirectionalShadows() {
  return _gameState->getLightManager()->getDirectionalShadows();
}

std::shared_ptr<BlurCompute> Core::getBloomBlur() { return _blurCompute; }

std::shared_ptr<Postprocessing> Core::getPostprocessing() { return _postprocessing; }

std::shared_ptr<EngineState> Core::getEngineState() { return _engineState; }

std::shared_ptr<GameState> Core::getGameState() { return _gameState; }

std::shared_ptr<Camera> Core::getCamera() { return _gameState->getCameraManager()->getCurrentCamera(); }

std::shared_ptr<GUI> Core::getGUI() { return _gui; }

std::tuple<int, int> Core::getFPS() { return {_timerFPSLimited->getFPS(), _timerFPSReal->getFPS()}; }