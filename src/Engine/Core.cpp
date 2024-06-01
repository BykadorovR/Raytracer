#include "Core.h"
#include <typeinfo>

Core::Core(std::shared_ptr<Settings> settings) { _state = std::make_shared<State>(settings); }

void Core::_initializeTextures() {
  auto settings = _state->getSettings();
  _textureRender.resize(settings->getMaxFramesInFlight());
  _textureBlurIn.resize(settings->getMaxFramesInFlight());
  _textureBlurOut.resize(settings->getMaxFramesInFlight());
  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    auto graphicImage = std::make_shared<Image>(
        settings->getResolution(), 1, 1, settings->getGraphicColorFormat(), VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _state);
    graphicImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                               _commandBufferTransfer);
    auto graphicImageView = std::make_shared<ImageView>(graphicImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                        VK_IMAGE_ASPECT_COLOR_BIT, _state);
    _textureRender[i] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, graphicImageView, _state);
    {
      auto blurImage = std::make_shared<Image>(settings->getResolution(), 1, 1, settings->getGraphicColorFormat(),
                                               VK_IMAGE_TILING_OPTIMAL,
                                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _state);
      blurImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                              _commandBufferTransfer);
      auto blurImageView = std::make_shared<ImageView>(blurImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                       VK_IMAGE_ASPECT_COLOR_BIT, _state);
      _textureBlurIn[i] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, blurImageView, _state);
    }
    {
      auto blurImage = std::make_shared<Image>(settings->getResolution(), 1, 1, settings->getGraphicColorFormat(),
                                               VK_IMAGE_TILING_OPTIMAL,
                                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _state);
      blurImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                              _commandBufferTransfer);
      auto blurImageView = std::make_shared<ImageView>(blurImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                       VK_IMAGE_ASPECT_COLOR_BIT, _state);
      _textureBlurOut[i] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, blurImageView, _state);
    }
  }
}

#ifdef __ANDROID__
void Core::setAssetManager(AAssetManager* assetManager) { _assetManager = assetManager; }
void Core::setNativeWindow(ANativeWindow* window) { _nativeWindow = window; }
#endif

void Core::initialize() {
#ifdef __ANDROID__
  _state->setNativeWindow(_nativeWindow);
  _state->setAssetManager(_assetManager);
#endif
  _state->initialize();
  auto settings = _state->getSettings();
  _swapchain = std::make_shared<Swapchain>(settings->getSwapchainColorFormat(), _state);
  _timer = std::make_shared<Timer>();
  _timerFPSReal = std::make_shared<TimerFPS>();
  _timerFPSLimited = std::make_shared<TimerFPS>();

  _commandPoolRender = std::make_shared<CommandPool>(QueueType::GRAPHIC, _state->getDevice());
  _commandBufferRender = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(), _commandPoolRender, _state);
  _commandPoolTransfer = std::make_shared<CommandPool>(QueueType::GRAPHIC, _state->getDevice());
  // frameInFlight != 0 can be used in reset
  _commandBufferTransfer = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(), _commandPoolTransfer,
                                                           _state);
  _commandPoolEquirectangular = std::make_shared<CommandPool>(QueueType::GRAPHIC, _state->getDevice());
  _commandBufferEquirectangular = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(),
                                                                  _commandPoolEquirectangular, _state);
  _commandPoolParticleSystem = std::make_shared<CommandPool>(QueueType::COMPUTE, _state->getDevice());
  _commandBufferParticleSystem = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(),
                                                                 _commandPoolParticleSystem, _state);
  _commandPoolPostprocessing = std::make_shared<CommandPool>(QueueType::COMPUTE, _state->getDevice());
  _commandBufferPostprocessing = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(),
                                                                 _commandPoolPostprocessing, _state);
  _commandPoolGUI = std::make_shared<CommandPool>(QueueType::GRAPHIC, _state->getDevice());
  _commandBufferGUI = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(), _commandPoolGUI, _state);

  _frameSubmitInfoPreCompute.resize(settings->getMaxFramesInFlight());
  _frameSubmitInfoGraphic.resize(settings->getMaxFramesInFlight());
  _frameSubmitInfoPostCompute.resize(settings->getMaxFramesInFlight());
  _frameSubmitInfoDebug.resize(settings->getMaxFramesInFlight());

  _renderPassGraphic = std::make_shared<RenderPass>(_state->getSettings(), _state->getDevice());
  _renderPassGraphic->initializeGraphic();
  _renderPassLightDepth = std::make_shared<RenderPass>(_state->getSettings(), _state->getDevice());
  _renderPassLightDepth->initializeLightDepth();
  _renderPassDebug = std::make_shared<RenderPass>(_state->getSettings(), _state->getDevice());
  _renderPassDebug->initializeDebug();

  // start transfer command buffer
  _commandBufferTransfer->beginCommands();

  _loggerGPU = std::make_shared<LoggerGPU>(_state);
  _loggerPostprocessing = std::make_shared<LoggerGPU>(_state);
  _loggerParticles = std::make_shared<LoggerGPU>(_state);
  _loggerGUI = std::make_shared<LoggerGPU>(_state);
  _loggerGPUDebug = std::make_shared<LoggerGPU>(_state);
  _loggerCPU = std::make_shared<LoggerCPU>();

  _resourceManager = std::make_shared<ResourceManager>(_commandBufferTransfer, _state);
#ifdef __ANDROID__
  _resourceManager->setAssetManager(_assetManager);
#endif
  _resourceManager->initialize();

  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    // graphic-presentation
    _semaphoreImageAvailable.push_back(std::make_shared<Semaphore>(_state->getDevice()));
    _semaphoreRenderFinished.push_back(std::make_shared<Semaphore>(_state->getDevice()));

    // compute-graphic
    _semaphoreParticleSystem.push_back(std::make_shared<Semaphore>(_state->getDevice()));
    _semaphoreGUI.push_back(std::make_shared<Semaphore>(_state->getDevice()));

    // postprocessing semaphore
    _semaphorePostprocessing.push_back(std::make_shared<Semaphore>(_state->getDevice()));
  }

  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    _fenceInFlight.push_back(std::make_shared<Fence>(_state->getDevice()));
  }

  _initializeTextures();

  _gui = std::make_shared<GUI>(_state);
  _gui->initialize(_commandBufferTransfer);

  _blur = std::make_shared<Blur>(_textureBlurIn, _textureBlurOut, _state);
  // for postprocessing descriptors GENERAL is needed
  _swapchain->overrideImageLayout(VK_IMAGE_LAYOUT_GENERAL);
  _postprocessing = std::make_shared<Postprocessing>(_textureRender, _textureBlurIn, _swapchain->getImageViews(),
                                                     _state);
  // but we expect it to be in VK_IMAGE_LAYOUT_PRESENT_SRC_KHR as start value
  _swapchain->changeImageLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, _commandBufferTransfer);
  _state->getInput()->subscribe(std::dynamic_pointer_cast<InputSubscriber>(_gui));

  _pool = std::make_shared<BS::thread_pool>(settings->getThreadsInPool());

  _lightManager = std::make_shared<LightManager>(_commandBufferTransfer, _resourceManager, _state);

  auto depthAttachment = std::make_shared<Image>(
      std::tuple{_swapchain->getSwapchainExtent().width, _swapchain->getSwapchainExtent().height}, 1, 1,
      settings->getDepthFormat(), VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _state);
  _depthAttachmentImageView = std::make_shared<ImageView>(depthAttachment, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                          VK_IMAGE_ASPECT_DEPTH_BIT, _state);

  _commandBufferTransfer->endCommands();

  _frameBufferGraphic.resize(_state->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    _frameBufferGraphic[i] = std::make_shared<Framebuffer>(
        std::vector{_textureRender[i]->getImageView(), _textureBlurIn[i]->getImageView(), _depthAttachmentImageView},
        _textureRender[i]->getImageView()->getImage()->getResolution(), _renderPassGraphic, _state->getDevice());
  }

  _frameBufferDebug.resize(_swapchain->getImageViews().size());
  for (int i = 0; i < _frameBufferDebug.size(); i++) {
    _frameBufferDebug[i] = std::make_shared<Framebuffer>(std::vector{_swapchain->getImageViews()[i]},
                                                         _swapchain->getImageViews()[i]->getImage()->getResolution(),
                                                         _renderPassDebug, _state->getDevice());
  }

  {
    // TODO: remove vkQueueWaitIdle, add fence or semaphore
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_commandBufferTransfer->getCommandBuffer()[_state->getFrameInFlight()];
    auto queue = _state->getDevice()->getQueue(QueueType::GRAPHIC);
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
  }
}

void Core::_directionalLightCalculator(int index) {
  auto frameInFlight = _state->getFrameInFlight();

  auto commandBuffer = _lightManager->getDirectionalLightCommandBuffers()[index];
  auto loggerGPU = _lightManager->getDirectionalLightLoggers()[index];

  // record command buffer
  commandBuffer->beginCommands();
  loggerGPU->setCommandBufferName("Directional command buffer", commandBuffer);

  loggerGPU->begin("Directional to depth buffer " + std::to_string(_timer->getFrameCounter()));
  //
  auto directionalLights = _lightManager->getDirectionalLights();

  auto renderPassInfo = _renderPassLightDepth->getRenderPassInfo(
      _frameBufferDirectionalLightDepth[index][frameInFlight]);
  VkClearValue clearDepth;
  clearDepth.depthStencil = {1.0f, 0};
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearDepth;

  // TODO: only one depth texture?
  vkCmdBeginRenderPass(commandBuffer->getCommandBuffer()[frameInFlight], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  // Set depth bias (aka "Polygon offset")
  // Required to avoid shadow mapping artifacts
  vkCmdSetDepthBias(commandBuffer->getCommandBuffer()[frameInFlight], _state->getSettings()->getDepthBiasConstant(),
                    0.0f, _state->getSettings()->getDepthBiasSlope());

  // draw scene here
  auto globalFrame = _timer->getFrameCounter();
  for (auto shadowable : _shadowables) {
    std::string drawableName = typeid(shadowable).name();
    loggerGPU->begin(drawableName + " to directional depth buffer " + std::to_string(globalFrame));
    shadowable->drawShadow(LightType::DIRECTIONAL, index, 0, commandBuffer);
    loggerGPU->end();
  }
  vkCmdEndRenderPass(commandBuffer->getCommandBuffer()[frameInFlight]);
  loggerGPU->end();

  // record command buffer
  commandBuffer->endCommands();
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer->getCommandBuffer()[frameInFlight];
  std::unique_lock<std::mutex> lock(_frameSubmitMutexGraphic);
  _frameSubmitInfoGraphic[frameInFlight].push_back(submitInfo);
}

void Core::_pointLightCalculator(int index, int face) {
  auto frameInFlight = _state->getFrameInFlight();

  auto commandBuffer = _lightManager->getPointLightCommandBuffers()[index][face];
  auto loggerGPU = _lightManager->getPointLightLoggers()[index][face];
  // record command buffer
  loggerGPU->setCommandBufferName("Point " + std::to_string(index) + "x" + std::to_string(face) + " command buffer",
                                  commandBuffer);
  commandBuffer->beginCommands();
  loggerGPU->begin("Point to depth buffer " + std::to_string(_timer->getFrameCounter()));
  auto renderPassInfo = _renderPassLightDepth->getRenderPassInfo(
      _frameBufferPointLightDepth[index][frameInFlight][face]);
  VkClearValue clearDepth;
  clearDepth.depthStencil = {1.0f, 0};
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearDepth;

  // TODO: only one depth texture?
  vkCmdBeginRenderPass(commandBuffer->getCommandBuffer()[frameInFlight], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  // Set depth bias (aka "Polygon offset")
  // Required to avoid shadow mapping artifacts
  vkCmdSetDepthBias(commandBuffer->getCommandBuffer()[frameInFlight], _state->getSettings()->getDepthBiasConstant(),
                    0.0f, _state->getSettings()->getDepthBiasSlope());

  // draw scene here
  auto globalFrame = _timer->getFrameCounter();
  float aspect = std::get<0>(_state->getSettings()->getResolution()) /
                 std::get<1>(_state->getSettings()->getResolution());

  // draw scene here
  for (auto shadowable : _shadowables) {
    std::string drawableName = typeid(shadowable).name();
    loggerGPU->begin(drawableName + " to point depth buffer " + std::to_string(globalFrame));
    shadowable->drawShadow(LightType::POINT, index, face, commandBuffer);
    loggerGPU->end();
  }
  vkCmdEndRenderPass(commandBuffer->getCommandBuffer()[frameInFlight]);
  loggerGPU->end();

  // record command buffer
  commandBuffer->endCommands();
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer->getCommandBuffer()[frameInFlight];
  std::unique_lock<std::mutex> lock(_frameSubmitMutexGraphic);
  _frameSubmitInfoGraphic[frameInFlight].push_back(submitInfo);
}

void Core::_computeParticles() {
  auto frameInFlight = _state->getFrameInFlight();

  _commandBufferParticleSystem->beginCommands();
  _loggerParticles->setCommandBufferName("Particles compute command buffer", _commandBufferParticleSystem);

  // any read from SSBO should wait for write to SSBO
  // First dispatch writes to a storage buffer, second dispatch reads from that storage buffer.
  VkMemoryBarrier memoryBarrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT};
  vkCmdPipelineBarrier(_commandBufferParticleSystem->getCommandBuffer()[frameInFlight],
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // srcStageMask
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // dstStageMask
                       0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

  _loggerParticles->begin("Particle system compute " + std::to_string(_timer->getFrameCounter()));
  for (auto& particleSystem : _particleSystem) {
    particleSystem->drawCompute(_commandBufferParticleSystem);
    particleSystem->updateTimer(_timer->getElapsedCurrent());
  }
  _loggerParticles->end();

  VkSubmitInfo submitInfoCompute{};
  submitInfoCompute.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfoCompute.signalSemaphoreCount = 1;
  submitInfoCompute.pSignalSemaphores = &_semaphoreParticleSystem[frameInFlight]->getSemaphore();
  submitInfoCompute.commandBufferCount = 1;
  submitInfoCompute.pCommandBuffers = &_commandBufferParticleSystem->getCommandBuffer()[frameInFlight];

  // end command buffer
  _commandBufferParticleSystem->endCommands();
  _frameSubmitInfoPreCompute[frameInFlight].push_back(submitInfoCompute);
}

void Core::_computePostprocessing(int swapchainImageIndex) {
  auto frameInFlight = _state->getFrameInFlight();

  _commandBufferPostprocessing->beginCommands();
  _loggerPostprocessing->setCommandBufferName("Postprocessing command buffer", _commandBufferPostprocessing);
  int bloomPasses = _state->getSettings()->getBloomPasses();
  // blur cycle:
  // in - out - horizontal
  // out - in - vertical
  for (int i = 0; i < bloomPasses; i++) {
    _loggerPostprocessing->begin("Blur horizontal compute " + std::to_string(_timer->getFrameCounter()));
    _blur->drawCompute(frameInFlight, true, _commandBufferPostprocessing);
    _loggerPostprocessing->end();

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

    _loggerPostprocessing->begin("Blur vertical compute " + std::to_string(_timer->getFrameCounter()));
    _blur->drawCompute(frameInFlight, false, _commandBufferPostprocessing);
    _loggerPostprocessing->end();

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

  _loggerPostprocessing->begin("Postprocessing compute " + std::to_string(_timer->getFrameCounter()));
  _postprocessing->drawCompute(frameInFlight, swapchainImageIndex, _commandBufferPostprocessing);
  _loggerPostprocessing->end();
  _commandBufferPostprocessing->endCommands();
}

void Core::_debugVisualizations(int swapchainImageIndex) {
  auto frameInFlight = _state->getFrameInFlight();

  _commandBufferGUI->beginCommands();
  _loggerGPUDebug->setCommandBufferName("GUI command buffer", _commandBufferGUI);

  auto renderPassInfo = _renderPassDebug->getRenderPassInfo(_frameBufferDebug[swapchainImageIndex]);
  VkClearValue clearColor;
  clearColor.color = _state->getSettings()->getClearColor();
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearColor;

  // TODO: only one depth texture?
  vkCmdBeginRenderPass(_commandBufferGUI->getCommandBuffer()[frameInFlight], &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
  _loggerGPUDebug->begin("Render GUI " + std::to_string(_timer->getFrameCounter()));
  _gui->updateBuffers(frameInFlight);
  _gui->drawFrame(frameInFlight, _commandBufferGUI);
  _loggerGPUDebug->end();
  vkCmdEndRenderPass(_commandBufferGUI->getCommandBuffer()[frameInFlight]);

  _commandBufferGUI->endCommands();
}

void Core::_renderGraphic() {
  auto frameInFlight = _state->getFrameInFlight();

  // record command buffer
  _commandBufferRender->beginCommands();
  _loggerGPU->setCommandBufferName("Draw command buffer", _commandBufferRender);

  /////////////////////////////////////////////////////////////////////////////////////////
  // depth to screne barrier
  /////////////////////////////////////////////////////////////////////////////////////////
  // Image memory barrier to make sure that writes are finished before sampling from the texture
  int directionalNum = _lightManager->getDirectionalLights().size();
  int pointNum = _lightManager->getPointLights().size();
  std::vector<VkImageMemoryBarrier> imageMemoryBarrier(directionalNum + pointNum);
  for (int i = 0; i < directionalNum; i++) {
    imageMemoryBarrier[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // We won't be changing the layout of the image
    imageMemoryBarrier[i].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    imageMemoryBarrier[i].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    imageMemoryBarrier[i].image = _lightManager->getDirectionalLights()[i]
                                      ->getDepthTexture()[frameInFlight]
                                      ->getImageView()
                                      ->getImage()
                                      ->getImage();
    imageMemoryBarrier[i].subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    imageMemoryBarrier[i].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    imageMemoryBarrier[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imageMemoryBarrier[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  }

  for (int i = 0; i < pointNum; i++) {
    int id = directionalNum + i;
    imageMemoryBarrier[id].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // We won't be changing the layout of the image
    imageMemoryBarrier[id].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    imageMemoryBarrier[id].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    imageMemoryBarrier[id].image = _lightManager->getPointLights()[i]
                                       ->getDepthCubemap()[frameInFlight]
                                       ->getTexture()
                                       ->getImageView()
                                       ->getImage()
                                       ->getImage();
    imageMemoryBarrier[id].subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    imageMemoryBarrier[id].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    imageMemoryBarrier[id].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imageMemoryBarrier[id].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier[id].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  }
  vkCmdPipelineBarrier(_commandBufferRender->getCommandBuffer()[frameInFlight],
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, imageMemoryBarrier.size(),
                       imageMemoryBarrier.data());

  /////////////////////////////////////////////////////////////////////////////////////////
  // render graphic
  /////////////////////////////////////////////////////////////////////////////////////////
  std::vector<VkClearValue> clearColor(3);
  clearColor[0].color = _state->getSettings()->getClearColor();
  clearColor[1].color = _state->getSettings()->getClearColor();
  clearColor[2].color = {1.0f, 0};

  auto renderPassInfo = _renderPassGraphic->getRenderPassInfo(_frameBufferGraphic[frameInFlight]);
  renderPassInfo.clearValueCount = 3;
  renderPassInfo.pClearValues = clearColor.data();

  auto globalFrame = _timer->getFrameCounter();
  // TODO: only one depth texture?
  vkCmdBeginRenderPass(_commandBufferRender->getCommandBuffer()[frameInFlight], &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  _loggerGPU->begin("Render light " + std::to_string(globalFrame));
  _lightManager->draw(frameInFlight);
  _loggerGPU->end();

  // draw scene here
  for (auto& animation : _animations) {
    if (_futureAnimationUpdate[animation].valid()) {
      _futureAnimationUpdate[animation].get();
    }
  }

  // first update materials
  for (auto& e : _materials) {
    e->update(frameInFlight);
  }

  // should be draw first
  if (_skybox) {
    _loggerGPU->begin("Render skybox " + std::to_string(globalFrame));
    _skybox->draw(_state->getSettings()->getResolution(), _camera, _commandBufferRender);
    _loggerGPU->end();
  }

  for (auto& drawable : _drawables[AlphaType::OPAQUE]) {
    // TODO: add getName() to drawable?
    std::string drawableName = typeid(drawable.get()).name();
    _loggerGPU->begin("Render " + drawableName + " " + std::to_string(globalFrame));
    drawable->draw(_state->getSettings()->getResolution(), _camera, _commandBufferRender);
    _loggerGPU->end();
  }

  std::sort(_drawables[AlphaType::TRANSPARENT].begin(), _drawables[AlphaType::TRANSPARENT].end(),
            [camera = _camera](std::shared_ptr<Drawable> left, std::shared_ptr<Drawable> right) {
              return glm::distance(glm::vec3(left->getModel()[3]), camera->getEye()) >
                     glm::distance(glm::vec3(right->getModel()[3]), camera->getEye());
            });
  for (auto& drawable : _drawables[AlphaType::TRANSPARENT]) {
    // TODO: add getName() to drawable?
    std::string drawableName = typeid(drawable.get()).name();
    _loggerGPU->begin("Render " + drawableName + " " + std::to_string(globalFrame));
    drawable->draw(_state->getSettings()->getResolution(), _camera, _commandBufferRender);
    _loggerGPU->end();
  }

  // submit model3D update
  for (auto& animation : _animations) {
    _futureAnimationUpdate[animation] = _pool->submit([&]() {
      _loggerCPU->begin("Update animation " + std::to_string(globalFrame));
      // we want update model for next frame, current frame we can't touch and update because it will be used on GPU
      animation->updateAnimation(_state->getFrameInFlight(), _timer->getElapsedCurrent());
      _loggerCPU->end();
    });
  }

  vkCmdEndRenderPass(_commandBufferRender->getCommandBuffer()[frameInFlight]);
  _commandBufferRender->endCommands();
}

VkResult Core::_getImageIndex(uint32_t* imageIndex) {
  auto frameInFlight = _state->getFrameInFlight();
  std::vector<VkFence> waitFences = {_fenceInFlight[frameInFlight]->getFence()};
  auto result = vkWaitForFences(_state->getDevice()->getLogicalDevice(), waitFences.size(), waitFences.data(), VK_TRUE,
                                UINT64_MAX);
  if (result != VK_SUCCESS) throw std::runtime_error("Can't wait for fence");

  _frameSubmitInfoPreCompute[frameInFlight].clear();
  _frameSubmitInfoGraphic[frameInFlight].clear();
  _frameSubmitInfoPostCompute[frameInFlight].clear();
  _frameSubmitInfoDebug[frameInFlight].clear();
  // RETURNS ONLY INDEX, NOT IMAGE
  // semaphore to signal, once image is available
  result = vkAcquireNextImageKHR(_state->getDevice()->getLogicalDevice(), _swapchain->getSwapchain(), UINT64_MAX,
                                 _semaphoreImageAvailable[frameInFlight]->getSemaphore(), VK_NULL_HANDLE, imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    _reset();
    return result;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  // Only reset the fence if we are submitting work
  result = vkResetFences(_state->getDevice()->getLogicalDevice(), waitFences.size(), waitFences.data());
  if (result != VK_SUCCESS) throw std::runtime_error("Can't reset fence");

  return result;
}

void Core::_reset() {
  auto surfaceCapabilities = _state->getDevice()->getSupportedSurfaceCapabilities();
  VkExtent2D extent = surfaceCapabilities.currentExtent;
  while (extent.width == 0 || extent.height == 0) {
    surfaceCapabilities = _state->getDevice()->getSupportedSurfaceCapabilities();
    extent = surfaceCapabilities.currentExtent;
#ifndef __ANDROID__
    glfwWaitEvents();
#endif
  }
  _swapchain->reset();
  _state->getSettings()->setResolution({extent.width, extent.height});
  _textureRender.clear();
  _textureBlurIn.clear();
  _textureBlurOut.clear();

  _commandBufferTransfer->beginCommands();

  _initializeTextures();
  _swapchain->overrideImageLayout(VK_IMAGE_LAYOUT_GENERAL);
  _postprocessing->reset(_textureRender, _textureBlurIn, _swapchain->getImageViews());
  _swapchain->changeImageLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, _commandBufferTransfer);
  _gui->reset();
  _blur->reset(_textureBlurIn, _textureBlurOut);

  _commandBufferTransfer->endCommands();

  {
    // TODO: remove vkQueueWaitIdle, add fence or semaphore
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_commandBufferTransfer->getCommandBuffer()[_state->getFrameInFlight()];
    auto queue = _state->getDevice()->getQueue(QueueType::GRAPHIC);
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
  }

  _callbackReset(extent.width, extent.height);
}

void Core::_drawFrame(int imageIndex) {
  auto frameInFlight = _state->getFrameInFlight();
  // submit compute particles
  auto particlesFuture = _pool->submit(std::bind(&Core::_computeParticles, this));

  /////////////////////////////////////////////////////////////////////////////////////////
  // render to depth buffer
  /////////////////////////////////////////////////////////////////////////////////////////
  std::vector<std::future<void>> shadowFutures;
  for (int i = 0; i < _lightManager->getDirectionalLights().size(); i++) {
    shadowFutures.push_back(_pool->submit(std::bind(&Core::_directionalLightCalculator, this, i)));
  }

  for (int i = 0; i < _lightManager->getPointLights().size(); i++) {
    for (int j = 0; j < 6; j++) {
      shadowFutures.push_back(_pool->submit(std::bind(&Core::_pointLightCalculator, this, i, j)));
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

  // wait for particles to complete before render
  if (particlesFuture.valid()) particlesFuture.get();
  {
    // binary wait structure
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT};

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &_semaphoreParticleSystem[frameInFlight]->getSemaphore();
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_commandBufferRender->getCommandBuffer()[frameInFlight];
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
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = waitSemaphoresPostprocessing.size();
    submitInfo.pWaitSemaphores = waitSemaphoresPostprocessing.data();
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &_semaphoreGUI[frameInFlight]->getSemaphore();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_commandBufferPostprocessing->getCommandBuffer()[frameInFlight];
    _frameSubmitInfoPostCompute[frameInFlight].push_back(submitInfo);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Render debug visualization
  //////////////////////////////////////////////////////////////////////////////////////////////////
  {
    if (debugVisualizationFuture.valid()) debugVisualizationFuture.get();

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &_semaphoreGUI[frameInFlight]->getSemaphore();
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_commandBufferGUI->getCommandBuffer()[frameInFlight];

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &_semaphoreRenderFinished[frameInFlight]->getSemaphore();
    _frameSubmitInfoDebug[frameInFlight].push_back(submitInfo);
  }

  // because of binary semaphores, we can't submit task with semaphore wait BEFORE task with semaphore signal.
  // that's why we have to split submit pipeline.
  vkQueueSubmit(_state->getDevice()->getQueue(QueueType::COMPUTE), _frameSubmitInfoPreCompute[frameInFlight].size(),
                _frameSubmitInfoPreCompute[frameInFlight].data(), VK_NULL_HANDLE);
  vkQueueSubmit(_state->getDevice()->getQueue(QueueType::GRAPHIC), _frameSubmitInfoGraphic[frameInFlight].size(),
                _frameSubmitInfoGraphic[frameInFlight].data(), VK_NULL_HANDLE);
  vkQueueSubmit(_state->getDevice()->getQueue(QueueType::COMPUTE), _frameSubmitInfoPostCompute[frameInFlight].size(),
                _frameSubmitInfoPostCompute[frameInFlight].data(), VK_NULL_HANDLE);
  // latest graphic job should signal fence about completion, otherwise render signal that it's done, but debug still in
  // progress (for 0 frame) and we submit new frame with 0 index
  vkQueueSubmit(_state->getDevice()->getQueue(QueueType::GRAPHIC), _frameSubmitInfoDebug[frameInFlight].size(),
                _frameSubmitInfoDebug[frameInFlight].data(), _fenceInFlight[frameInFlight]->getFence());
}

void Core::_displayFrame(uint32_t* imageIndex) {
  auto frameInFlight = _state->getFrameInFlight();

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  std::vector<VkSemaphore> waitSemaphoresPresent = {_semaphoreRenderFinished[frameInFlight]->getSemaphore()};

  presentInfo.waitSemaphoreCount = waitSemaphoresPresent.size();
  presentInfo.pWaitSemaphores = waitSemaphoresPresent.data();

  VkSwapchainKHR swapChains[] = {_swapchain->getSwapchain()};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;

  presentInfo.pImageIndices = imageIndex;

  // TODO: change to own present queue
  auto result = vkQueuePresentKHR(_state->getDevice()->getQueue(QueueType::PRESENT), &presentInfo);

  // getResized() can be valid only here, we can get inconsistencies in semaphores if VK_ERROR_OUT_OF_DATE_KHR is not
  // reported by Vulkan
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _state->getWindow()->getResized()) {
    _state->getWindow()->setResized(false);
    _reset();
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }
}

void Core::draw() {
#ifdef __ANDROID__
  {
#else
  while (!glfwWindowShouldClose((GLFWwindow*)(_state->getWindow()->getWindow()))) {
    glfwPollEvents();
#endif
    _timer->tick();
    _timerFPSReal->tick();
    _timerFPSLimited->tick();
    _state->setFrameInFlight(_timer->getFrameCounter() % _state->getSettings()->getMaxFramesInFlight());

    // business/application update loop callback
    uint32_t imageIndex;
    while (_getImageIndex(&imageIndex) != VK_SUCCESS)
      ;
    _callbackUpdate();
    _drawFrame(imageIndex);
    _timerFPSReal->tock();
    // if GPU frames are limited by driver it will happen during display
    _displayFrame(&imageIndex);

    _timer->sleep(_state->getSettings()->getDesiredFPS(), _loggerCPU);
    _timer->tock();
    _timerFPSLimited->tock();
  }
#ifndef __ANDROID__
  vkDeviceWaitIdle(_state->getDevice()->getLogicalDevice());
#endif
}

void Core::registerUpdate(std::function<void()> update) { _callbackUpdate = update; }

void Core::registerReset(std::function<void(int width, int height)> reset) { _callbackReset = reset; }

void Core::startRecording() { _commandBufferTransfer->beginCommands(); }

void Core::endRecording() {
  _commandBufferTransfer->endCommands();
  // TODO: remove vkQueueWaitIdle, add fence or semaphore
  // TODO: move this function to core
  {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_commandBufferTransfer->getCommandBuffer()[0];
    auto queue = _state->getDevice()->getQueue(QueueType::GRAPHIC);
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
  }
}

void Core::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

void Core::addDrawable(std::shared_ptr<Drawable> drawable, AlphaType type) { _drawables[type].push_back(drawable); }

void Core::addShadowable(std::shared_ptr<Shadowable> shadowable) { _shadowables.push_back(shadowable); }

void Core::addSkybox(std::shared_ptr<Skybox> skybox) { _skybox = skybox; }

void Core::addParticleSystem(std::shared_ptr<ParticleSystem> particleSystem) {
  _particleSystem.push_back(particleSystem);
  addDrawable(particleSystem);
}

void Core::removeDrawable(std::shared_ptr<Drawable> drawable) {
  for (auto& [_, drawableVector] : _drawables)
    drawableVector.erase(std::remove(drawableVector.begin(), drawableVector.end(), drawable), drawableVector.end());
}

std::tuple<std::shared_ptr<uint8_t[]>, std::tuple<int, int, int>> Core::loadImageCPU(std::string path) {
  return _resourceManager->loadImageCPU<uint8_t>(path);
}

std::shared_ptr<BufferImage> Core::loadImageGPU(std::string path) {
  return _resourceManager->loadImageGPU<uint8_t>({path});
}

std::shared_ptr<Texture> Core::createTexture(std::string name, VkFormat format, int mipMapLevels) {
  auto texture = std::make_shared<Texture>(_resourceManager->loadImageGPU<uint8_t>({name}), format,
                                           VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, _commandBufferTransfer,
                                           _state);
  return texture;
}

std::shared_ptr<Cubemap> Core::createCubemap(std::vector<std::string> paths, VkFormat format, int mipMapLevels) {
  return std::make_shared<Cubemap>(
      _resourceManager->loadImageGPU<uint8_t>(paths), format, mipMapLevels, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      _commandBufferTransfer, _state);
}

std::shared_ptr<ModelGLTF> Core::createModelGLTF(std::string path) {
  auto model = _resourceManager->loadModel(path);
  _materials.insert(model->getMaterialsColor().begin(), model->getMaterialsColor().end());
  _materials.insert(model->getMaterialsPhong().begin(), model->getMaterialsPhong().end());
  _materials.insert(model->getMaterialsPBR().begin(), model->getMaterialsPBR().end());
  return model;
}

std::shared_ptr<Animation> Core::createAnimation(std::shared_ptr<ModelGLTF> modelGLTF) {
  auto animation = std::make_shared<Animation>(modelGLTF->getNodes(), modelGLTF->getSkins(), modelGLTF->getAnimations(),
                                               _state);
  _animations.push_back(animation);
  return animation;
}

std::shared_ptr<Equirectangular> Core::createEquirectangular(std::string path) {
  return std::make_shared<Equirectangular>(path, _commandBufferTransfer, _resourceManager, _state);
}

std::shared_ptr<MaterialColor> Core::createMaterialColor(MaterialTarget target) {
  auto material = std::make_shared<MaterialColor>(target, _commandBufferTransfer, _state);
  _materials.insert(material);
  return material;
}

std::shared_ptr<MaterialPhong> Core::createMaterialPhong(MaterialTarget target) {
  auto material = std::make_shared<MaterialPhong>(target, _commandBufferTransfer, _state);
  _materials.insert(material);
  return material;
}

std::shared_ptr<MaterialPBR> Core::createMaterialPBR(MaterialTarget target) {
  auto material = std::make_shared<MaterialPBR>(target, _commandBufferTransfer, _state);
  _materials.insert(material);
  return material;
}

std::shared_ptr<Shape3D> Core::createShape3D(ShapeType shapeType, VkCullModeFlagBits cullMode) {
  auto shape = std::make_shared<Shape3D>(shapeType, cullMode, _lightManager, _commandBufferTransfer, _resourceManager,
                                         _state);
  return shape;
}

std::shared_ptr<Model3D> Core::createModel3D(std::shared_ptr<ModelGLTF> modelGLTF) {
  return std::make_shared<Model3D>(modelGLTF->getNodes(), modelGLTF->getMeshes(), _lightManager, _commandBufferTransfer,
                                   _resourceManager, _state);
}

std::shared_ptr<Sprite> Core::createSprite() {
  return std::make_shared<Sprite>(_lightManager, _commandBufferTransfer, _resourceManager, _state);
}

std::shared_ptr<Terrain> Core::createTerrain(std::string heightmap, std::pair<int, int> patches) {
  return std::make_shared<Terrain>(_resourceManager->loadImageGPU<uint8_t>({heightmap}), patches,
                                   _commandBufferTransfer, _lightManager, _state);
}

std::shared_ptr<Line> Core::createLine() { return std::make_shared<Line>(_commandBufferTransfer, _state); }

std::shared_ptr<IBL> Core::createIBL() {
  return std::make_shared<IBL>(_lightManager, _commandBufferTransfer, _resourceManager, _state);
}

std::shared_ptr<ParticleSystem> Core::createParticleSystem(std::vector<Particle> particles,
                                                           std::shared_ptr<Texture> particleTexture) {
  return std::make_shared<ParticleSystem>(particles, particleTexture, _commandBufferTransfer, _state);
}

std::shared_ptr<Skybox> Core::createSkybox() {
  return std::make_shared<Skybox>(_commandBufferTransfer, _resourceManager, _state);
}

std::shared_ptr<PointLight> Core::createPointLight(std::tuple<int, int> resolution) {
  auto light = _lightManager->createPointLight(resolution);
  std::vector<std::vector<std::shared_ptr<Framebuffer>>> pointLightDepth(_state->getSettings()->getMaxFramesInFlight());
  auto cubemap = light->getDepthCubemap();
  for (int j = 0; j < _state->getSettings()->getMaxFramesInFlight(); j++) {
    std::vector<std::shared_ptr<Framebuffer>> pointLightFaces(6);
    for (int i = 0; i < 6; i++) {
      auto imageView = cubemap[j]->getTextureSeparate()[i][0]->getImageView();
      pointLightFaces[i] = std::make_shared<Framebuffer>(std::vector{imageView}, imageView->getImage()->getResolution(),
                                                         _renderPassLightDepth, _state->getDevice());
    }
    pointLightDepth[j] = pointLightFaces;
  }
  _frameBufferPointLightDepth.push_back(pointLightDepth);
  return light;
}

std::shared_ptr<DirectionalLight> Core::createDirectionalLight(std::tuple<int, int> resolution) {
  auto light = _lightManager->createDirectionalLight(resolution);
  std::vector<std::shared_ptr<Framebuffer>> directionalLightDepth(_state->getSettings()->getMaxFramesInFlight());
  auto textures = light->getDepthTexture();
  for (int j = 0; j < _state->getSettings()->getMaxFramesInFlight(); j++) {
    directionalLightDepth[j] = std::make_shared<Framebuffer>(std::vector{textures[j]->getImageView()},
                                                             textures[j]->getImageView()->getImage()->getResolution(),
                                                             _renderPassLightDepth, _state->getDevice());
  }
  _frameBufferDirectionalLightDepth.push_back(directionalLightDepth);
  return light;
}

std::shared_ptr<AmbientLight> Core::createAmbientLight() { return _lightManager->createAmbientLight(); }

std::shared_ptr<CommandBuffer> Core::getCommandBufferTransfer() { return _commandBufferTransfer; }

std::shared_ptr<ResourceManager> Core::getResourceManager() { return _resourceManager; }

const std::vector<std::shared_ptr<Drawable>>& Core::getDrawables(AlphaType type) { return _drawables[type]; }

std::vector<std::shared_ptr<PointLight>> Core::getPointLights() { return _lightManager->getPointLights(); }

std::vector<std::shared_ptr<DirectionalLight>> Core::getDirectionalLights() {
  return _lightManager->getDirectionalLights();
}

std::shared_ptr<Postprocessing> Core::getPostprocessing() { return _postprocessing; }

std::shared_ptr<Blur> Core::getBlur() { return _blur; }

std::shared_ptr<State> Core::getState() { return _state; }

std::shared_ptr<GUI> Core::getGUI() { return _gui; }

std::tuple<int, int> Core::getFPS() { return {_timerFPSLimited->getFPS(), _timerFPSReal->getFPS()}; }