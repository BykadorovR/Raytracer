#include "Core.h"
#include <typeinfo>

Core::Core(std::shared_ptr<Settings> settings) {
  _state = std::make_shared<State>(settings);
  _timer = std::make_shared<Timer>();
  _timerFPSReal = std::make_shared<TimerFPS>();
  _timerFPSLimited = std::make_shared<TimerFPS>();

  _commandPoolRender = std::make_shared<CommandPool>(QueueType::GRAPHIC, _state->getDevice());
  _commandBufferRender = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(), _commandPoolRender,
                                                         _state->getDevice());
  _commandPoolTransfer = std::make_shared<CommandPool>(QueueType::GRAPHIC, _state->getDevice());
  _commandBufferTransfer = std::make_shared<CommandBuffer>(1, _commandPoolTransfer, _state->getDevice());
  _commandPoolEquirectangular = std::make_shared<CommandPool>(QueueType::GRAPHIC, _state->getDevice());
  _commandBufferEquirectangular = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(),
                                                                  _commandPoolEquirectangular, _state->getDevice());
  _commandPoolParticleSystem = std::make_shared<CommandPool>(QueueType::COMPUTE, _state->getDevice());
  _commandBufferParticleSystem = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(),
                                                                 _commandPoolParticleSystem, _state->getDevice());
  _commandPoolPostprocessing = std::make_shared<CommandPool>(QueueType::COMPUTE, _state->getDevice());
  _commandBufferPostprocessing = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(),
                                                                 _commandPoolPostprocessing, _state->getDevice());
  _commandPoolGUI = std::make_shared<CommandPool>(QueueType::GRAPHIC, _state->getDevice());
  _commandBufferGUI = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(), _commandPoolGUI,
                                                      _state->getDevice());

  // start transfer command buffer
  _commandBufferTransfer->beginCommands(0);

  _loggerGPU = std::make_shared<LoggerGPU>(_state);
  _loggerPostprocessing = std::make_shared<LoggerGPU>(_state);
  _loggerParticles = std::make_shared<LoggerGPU>(_state);
  _loggerGUI = std::make_shared<LoggerGPU>(_state);
  _loggerGPUDebug = std::make_shared<LoggerGPU>(_state);
  _loggerCPU = std::make_shared<LoggerCPU>();

  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    // graphic-presentation
    _semaphoreImageAvailable.push_back(std::make_shared<Semaphore>(VK_SEMAPHORE_TYPE_BINARY, _state->getDevice()));
    _semaphoreRenderFinished.push_back(std::make_shared<Semaphore>(VK_SEMAPHORE_TYPE_BINARY, _state->getDevice()));

    // compute-graphic
    _semaphoreParticleSystem.push_back(std::make_shared<Semaphore>(VK_SEMAPHORE_TYPE_TIMELINE, _state->getDevice()));
    _semaphorePostprocessing.push_back(std::make_shared<Semaphore>(VK_SEMAPHORE_TYPE_BINARY, _state->getDevice()));
    _semaphoreGUI.push_back(std::make_shared<Semaphore>(VK_SEMAPHORE_TYPE_BINARY, _state->getDevice()));
  }

  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    _fenceInFlight.push_back(std::make_shared<Fence>(_state->getDevice()));
    _fenceParticleSystem.push_back(std::make_shared<Fence>(_state->getDevice()));
  }

  _textureRender.resize(settings->getMaxFramesInFlight());
  _textureBlurIn.resize(settings->getMaxFramesInFlight());
  _textureBlurOut.resize(settings->getMaxFramesInFlight());
  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    auto graphicImage = std::make_shared<Image>(settings->getResolution(), 1, 1, settings->getGraphicColorFormat(),
                                                VK_IMAGE_TILING_OPTIMAL,
                                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _state->getDevice());
    graphicImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                               _commandBufferTransfer);
    auto graphicImageView = std::make_shared<ImageView>(graphicImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                        VK_IMAGE_ASPECT_COLOR_BIT, _state->getDevice());
    _textureRender[i] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, graphicImageView, _state);
    {
      auto blurImage = std::make_shared<Image>(settings->getResolution(), 1, 1, settings->getGraphicColorFormat(),
                                               VK_IMAGE_TILING_OPTIMAL,
                                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _state->getDevice());
      blurImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                              _commandBufferTransfer);
      auto blurImageView = std::make_shared<ImageView>(blurImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                       VK_IMAGE_ASPECT_COLOR_BIT, _state->getDevice());
      _textureBlurIn[i] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, blurImageView, _state);
    }
    {
      auto blurImage = std::make_shared<Image>(settings->getResolution(), 1, 1, settings->getGraphicColorFormat(),
                                               VK_IMAGE_TILING_OPTIMAL,
                                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _state->getDevice());
      blurImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                              _commandBufferTransfer);
      auto blurImageView = std::make_shared<ImageView>(blurImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                       VK_IMAGE_ASPECT_COLOR_BIT, _state->getDevice());
      _textureBlurOut[i] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, blurImageView, _state);
    }
  }

  _gui = std::make_shared<GUI>(_state);
  _gui->initialize(_commandBufferTransfer);

  _blur = std::make_shared<Blur>(_textureBlurIn, _textureBlurOut, _state);
  // for postprocessing descriptors GENERAL is needed
  _state->getSwapchain()->overrideImageLayout(VK_IMAGE_LAYOUT_GENERAL);
  _postprocessing = std::make_shared<Postprocessing>(_textureRender, _textureBlurIn,
                                                     _state->getSwapchain()->getImageViews(), _state);
  // but we expect it to be in VK_IMAGE_LAYOUT_PRESENT_SRC_KHR as start value
  _state->getSwapchain()->changeImageLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, _commandBufferTransfer);
  _state->getInput()->subscribe(std::dynamic_pointer_cast<InputSubscriber>(_gui));

  _pool = std::make_shared<BS::thread_pool>(settings->getThreadsInPool());

  _lightManager = std::make_shared<LightManager>(_commandBufferTransfer, _state);

  _commandBufferTransfer->endCommands();
  _commandBufferTransfer->submitToQueue(true);
}

void Core::_directionalLightCalculator(int index) {
  auto commandBuffer = _commandBufferDirectional[index];
  auto loggerGPU = _loggerGPUDirectional[index];

  // record command buffer
  commandBuffer->beginCommands(_currentFrame);
  loggerGPU->setCommandBufferName("Directional command buffer", _currentFrame, commandBuffer);

  loggerGPU->begin("Directional to depth buffer " + std::to_string(_timer->getFrameCounter()), _currentFrame);
  // change layout to write one
  VkImageMemoryBarrier imageMemoryBarrier{};
  imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  // We won't be changing the layout of the image
  imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  imageMemoryBarrier.image = _lightManager->getDirectionalLights()[index]
                                 ->getDepthTexture()[_currentFrame]
                                 ->getImageView()
                                 ->getImage()
                                 ->getImage();
  imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
  imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[_currentFrame],
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &imageMemoryBarrier);
  //
  auto directionalLights = _lightManager->getDirectionalLights();
  VkClearValue clearDepth;
  clearDepth.depthStencil = {1.0f, 0};
  const VkRenderingAttachmentInfo depthAttachmentInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = directionalLights[index]->getDepthTexture()[_currentFrame]->getImageView()->getImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearDepth,
  };

  auto [width, height] =
      directionalLights[index]->getDepthTexture()[_currentFrame]->getImageView()->getImage()->getResolution();
  VkRect2D renderArea{};
  renderArea.extent.width = width;
  renderArea.extent.height = height;
  const VkRenderingInfoKHR renderInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
      .renderArea = renderArea,
      .layerCount = 1,
      .pDepthAttachment = &depthAttachmentInfo,
  };

  // TODO: only one depth texture?
  vkCmdBeginRendering(commandBuffer->getCommandBuffer()[_currentFrame], &renderInfo);
  // Set depth bias (aka "Polygon offset")
  // Required to avoid shadow mapping artifacts
  vkCmdSetDepthBias(commandBuffer->getCommandBuffer()[_currentFrame], _state->getSettings()->getDepthBiasConstant(),
                    0.0f, _state->getSettings()->getDepthBiasSlope());

  // draw scene here
  auto globalFrame = _timer->getFrameCounter();
  for (auto shadowable : _shadowables) {
    std::string drawableName = typeid(shadowable).name();
    loggerGPU->begin(drawableName + " to directional depth buffer " + std::to_string(globalFrame), _currentFrame);
    shadowable->drawShadow(_currentFrame, commandBuffer, LightType::DIRECTIONAL, index);
    loggerGPU->end();
  }
  vkCmdEndRendering(commandBuffer->getCommandBuffer()[_currentFrame]);
  loggerGPU->end();

  // record command buffer
  commandBuffer->endCommands();
  commandBuffer->submitToQueue(false);
}

void Core::_pointLightCalculator(int index, int face) {
  auto commandBuffer = _commandBufferPoint[index][face];
  auto loggerGPU = _loggerGPUPoint[index][face];
  // record command buffer
  loggerGPU->setCommandBufferName("Point " + std::to_string(index) + "x" + std::to_string(face) + " command buffer",
                                  _currentFrame, commandBuffer);
  commandBuffer->beginCommands(_currentFrame);
  loggerGPU->begin("Point to depth buffer " + std::to_string(_timer->getFrameCounter()), _currentFrame);
  // cubemap is the only image, rest is image views, so we need to perform change only once
  if (face == 0) {
    // change layout to write one
    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // We won't be changing the layout of the image
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    imageMemoryBarrier.image = _lightManager->getPointLights()[index]
                                   ->getDepthCubemap()[_currentFrame]
                                   ->getTexture()
                                   ->getImageView()
                                   ->getImage()
                                   ->getImage();
    imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[_currentFrame],
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &imageMemoryBarrier);
  }
  auto pointLights = _lightManager->getPointLights();
  VkClearValue clearDepth;
  clearDepth.depthStencil = {1.0f, 0};
  const VkRenderingAttachmentInfo depthAttachmentInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = pointLights[index]
                       ->getDepthCubemap()[_currentFrame]
                       ->getTextureSeparate()[face][0]
                       ->getImageView()
                       ->getImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearDepth,
  };

  auto [width, height] = pointLights[index]
                             ->getDepthCubemap()[_currentFrame]
                             ->getTextureSeparate()[face][0]
                             ->getImageView()
                             ->getImage()
                             ->getResolution();
  VkRect2D renderArea{};
  renderArea.extent.width = width;
  renderArea.extent.height = height;
  const VkRenderingInfo renderInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = renderArea,
      .layerCount = 1,
      .pDepthAttachment = &depthAttachmentInfo,
  };

  // TODO: only one depth texture?
  vkCmdBeginRendering(commandBuffer->getCommandBuffer()[_currentFrame], &renderInfo);
  // Set depth bias (aka "Polygon offset")
  // Required to avoid shadow mapping artifacts
  vkCmdSetDepthBias(commandBuffer->getCommandBuffer()[_currentFrame], _state->getSettings()->getDepthBiasConstant(),
                    0.0f, _state->getSettings()->getDepthBiasSlope());

  // draw scene here
  auto globalFrame = _timer->getFrameCounter();
  float aspect = std::get<0>(_state->getSettings()->getResolution()) /
                 std::get<1>(_state->getSettings()->getResolution());

  // draw scene here
  for (auto shadowable : _shadowables) {
    std::string drawableName = typeid(shadowable).name();
    loggerGPU->begin(drawableName + " to point depth buffer " + std::to_string(globalFrame), _currentFrame);
    shadowable->drawShadow(_currentFrame, commandBuffer, LightType::POINT, index);
    loggerGPU->end();
  }
  vkCmdEndRendering(commandBuffer->getCommandBuffer()[_currentFrame]);
  loggerGPU->end();

  // record command buffer
  commandBuffer->endCommands();
  commandBuffer->submitToQueue(false);
}

void Core::_computeParticles() {
  _commandBufferParticleSystem->beginCommands(_currentFrame);
  _loggerParticles->setCommandBufferName("Particles compute command buffer", _currentFrame,
                                         _commandBufferParticleSystem);

  // any read from SSBO should wait for write to SSBO
  // First dispatch writes to a storage buffer, second dispatch reads from that storage buffer.
  VkMemoryBarrier memoryBarrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT};
  vkCmdPipelineBarrier(_commandBufferParticleSystem->getCommandBuffer()[_currentFrame],
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // srcStageMask
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // dstStageMask
                       0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

  _loggerParticles->begin("Particle system compute " + std::to_string(_timer->getFrameCounter()), _currentFrame);
  for (auto& particleSystem : _particleSystem) {
    particleSystem->drawCompute(_currentFrame, _commandBufferParticleSystem);
    particleSystem->updateTimer(_timer->getElapsedCurrent());
  }
  _loggerParticles->end();

  auto particleSignal = _timer->getFrameCounter() + 1;
  VkTimelineSemaphoreSubmitInfo timelineInfo{};
  timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
  timelineInfo.pNext = NULL;
  timelineInfo.signalSemaphoreValueCount = 1;
  timelineInfo.pSignalSemaphoreValues = &particleSignal;

  VkSubmitInfo submitInfoCompute{};
  submitInfoCompute.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfoCompute.pNext = &timelineInfo;
  VkSemaphore signalParticleSemaphores[] = {_semaphoreParticleSystem[_currentFrame]->getSemaphore()};
  submitInfoCompute.signalSemaphoreCount = 1;
  submitInfoCompute.pSignalSemaphores = signalParticleSemaphores;
  submitInfoCompute.commandBufferCount = 1;
  submitInfoCompute.pCommandBuffers = &_commandBufferParticleSystem->getCommandBuffer()[_currentFrame];

  // end command buffer
  _commandBufferParticleSystem->endCommands();
  _commandBufferParticleSystem->submitToQueue(submitInfoCompute, nullptr);
}

void Core::_computePostprocessing(int swapchainImageIndex) {
  _commandBufferPostprocessing->beginCommands(_currentFrame);
  _loggerPostprocessing->setCommandBufferName("Postprocessing command buffer", _currentFrame,
                                              _commandBufferPostprocessing);
  int bloomPasses = _state->getSettings()->getBloomPasses();
  // blur cycle:
  // in - out - horizontal
  // out - in - vertical
  for (int i = 0; i < bloomPasses; i++) {
    _loggerPostprocessing->begin("Blur horizontal compute " + std::to_string(_timer->getFrameCounter()), _currentFrame);
    _blur->drawCompute(_currentFrame, true, _commandBufferPostprocessing);
    _loggerPostprocessing->end();

    // sync between horizontal and vertical
    // wait dst (textureOut) to be written
    {
      VkImageMemoryBarrier colorBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                                        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                                        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                        .image = _textureBlurOut[_currentFrame]->getImageView()->getImage()->getImage(),
                                        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                             .baseMipLevel = 0,
                                                             .levelCount = 1,
                                                             .baseArrayLayer = 0,
                                                             .layerCount = 1}};
      vkCmdPipelineBarrier(_commandBufferPostprocessing->getCommandBuffer()[_currentFrame],
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // srcStageMask
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // dstStageMask
                           0, 0, nullptr, 0, nullptr,
                           1,             // imageMemoryBarrierCount
                           &colorBarrier  // pImageMemoryBarriers
      );
    }

    _loggerPostprocessing->begin("Blur vertical compute " + std::to_string(_timer->getFrameCounter()), _currentFrame);
    _blur->drawCompute(_currentFrame, false, _commandBufferPostprocessing);
    _loggerPostprocessing->end();

    // wait blur image to be ready
    // blurTextureIn stores result after blur (vertical part, out - in)
    {
      VkImageMemoryBarrier colorBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                                        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                                        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                        .image = _textureBlurIn[_currentFrame]->getImageView()->getImage()->getImage(),
                                        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                             .baseMipLevel = 0,
                                                             .levelCount = 1,
                                                             .baseArrayLayer = 0,
                                                             .layerCount = 1}};
      vkCmdPipelineBarrier(_commandBufferPostprocessing->getCommandBuffer()[_currentFrame],
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
    VkImageMemoryBarrier colorBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = _state->getSwapchain()->getImageViews()[swapchainImageIndex]->getImage()->getImage(),
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};
    vkCmdPipelineBarrier(_commandBufferPostprocessing->getCommandBuffer()[_currentFrame],
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,     // srcStageMask
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // dstStageMask
                         0, 0, nullptr, 0, nullptr,
                         1,             // imageMemoryBarrierCount
                         &colorBarrier  // pImageMemoryBarriers
    );
  }

  _loggerPostprocessing->begin("Postprocessing compute " + std::to_string(_timer->getFrameCounter()), _currentFrame);
  _postprocessing->drawCompute(_currentFrame, swapchainImageIndex, _commandBufferPostprocessing);
  _loggerPostprocessing->end();
  _commandBufferPostprocessing->endCommands();
}

void Core::_debugVisualizations(int swapchainImageIndex) {
  _commandBufferGUI->beginCommands(_currentFrame);
  _loggerGPUDebug->setCommandBufferName("GUI command buffer", _currentFrame, _commandBufferGUI);

  _loggerGPUDebug->begin("Change layout before display", _currentFrame);
  VkImageMemoryBarrier colorBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
      .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .image = _state->getSwapchain()->getImageViews()[swapchainImageIndex]->getImage()->getImage(),
      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};

  vkCmdPipelineBarrier(_commandBufferGUI->getCommandBuffer()[_currentFrame],
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // srcStageMask
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,  // dstStageMask
                       0, 0, nullptr, 0, nullptr,
                       1,             // imageMemoryBarrierCount
                       &colorBarrier  // pImageMemoryBarriers
  );
  _loggerGPUDebug->end();

  _commandBufferGUI->endCommands();
}

void Core::_renderGraphic() {
  // record command buffer
  _commandBufferRender->beginCommands(_currentFrame);
  _loggerGPU->setCommandBufferName("Draw command buffer", _currentFrame, _commandBufferRender);

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
    imageMemoryBarrier[i].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    imageMemoryBarrier[i].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    imageMemoryBarrier[i].image = _lightManager->getDirectionalLights()[i]
                                      ->getDepthTexture()[_currentFrame]
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
    imageMemoryBarrier[id].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    imageMemoryBarrier[id].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    imageMemoryBarrier[id].image = _lightManager->getPointLights()[i]
                                       ->getDepthCubemap()[_currentFrame]
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
  vkCmdPipelineBarrier(_commandBufferRender->getCommandBuffer()[_currentFrame],
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, imageMemoryBarrier.size(),
                       imageMemoryBarrier.data());

  /////////////////////////////////////////////////////////////////////////////////////////
  // render graphic
  /////////////////////////////////////////////////////////////////////////////////////////
  VkClearValue clearColor;
  clearColor.color = _state->getSettings()->getClearColor();
  std::vector<VkRenderingAttachmentInfo> colorAttachmentInfo(2);
  // here we render scene as is
  colorAttachmentInfo[0] = VkRenderingAttachmentInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = _textureRender[_currentFrame]->getImageView()->getImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearColor,
  };
  // here we render bloom that will be applied on postprocessing stage to simple render
  colorAttachmentInfo[1] = VkRenderingAttachmentInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = _textureBlurIn[_currentFrame]->getImageView()->getImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearColor,
  };

  VkClearValue clearDepth;
  clearDepth.depthStencil = {1.0f, 0};
  const VkRenderingAttachmentInfo depthAttachmentInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = _state->getSwapchain()->getDepthImageView()->getImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearDepth,
  };

  auto [width, height] = _textureRender[_currentFrame]->getImageView()->getImage()->getResolution();
  VkRect2D renderArea{};
  renderArea.extent.width = width;
  renderArea.extent.height = height;
  const VkRenderingInfo renderInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = renderArea,
      .layerCount = 1,
      .colorAttachmentCount = (uint32_t)colorAttachmentInfo.size(),
      .pColorAttachments = colorAttachmentInfo.data(),
      .pDepthAttachment = &depthAttachmentInfo,
  };

  auto globalFrame = _timer->getFrameCounter();
  vkCmdBeginRendering(_commandBufferRender->getCommandBuffer()[_currentFrame], &renderInfo);
  _loggerGPU->begin("Render light " + std::to_string(globalFrame), _currentFrame);
  _lightManager->draw(_currentFrame);
  _loggerGPU->end();

  // draw scene here
  // TODO: should be just before ModelManagers
  for (auto& animation : _animations) {
    if (_futureAnimationUpdate[animation].valid()) {
      _futureAnimationUpdate[animation].get();
    }
  }

  for (auto& drawable : _drawables) {
    // TODO: add getName() to drawable?
    std::string drawableName = typeid(drawable.get()).name();
    _loggerGPU->begin("Render " + drawableName + " " + std::to_string(globalFrame), _currentFrame);
    drawable->draw(_currentFrame, _state->getSettings()->getResolution(), _commandBufferRender,
                   _state->getSettings()->getDrawType());
    _loggerGPU->end();
  }

  // submit model3D update
  for (auto& animation : _animations) {
    _futureAnimationUpdate[animation] = _pool->submit([&]() {
      _loggerCPU->begin("Update animation " + std::to_string(globalFrame));
      // we want update model for next frame, current frame we can't touch and update because it will be used on GPU
      animation->updateAnimation(1 - _currentFrame, _timer->getElapsedCurrent());
      _loggerCPU->end();
    });
  }
  // contains transparency, should be drawn last
  _loggerGPU->begin("Render particles " + std::to_string(globalFrame), _currentFrame);
  for (auto& particleSystem : _particleSystem) {
    particleSystem->drawGraphic(_currentFrame, _commandBufferRender);
  }
  _loggerGPU->end();

  // should be drawn last
  if (_skybox) {
    _loggerGPU->begin("Render skybox " + std::to_string(globalFrame), _currentFrame);
    _skybox->draw(_currentFrame, _commandBufferRender);
    _loggerGPU->end();
  }

  vkCmdEndRendering(_commandBufferRender->getCommandBuffer()[_currentFrame]);
  _commandBufferRender->endCommands();
}

void Core::_getImageIndex(uint32_t* imageIndex) {
  _currentFrame = _timer->getFrameCounter() % _state->getSettings()->getMaxFramesInFlight();
  std::vector<VkFence> waitFences = {_fenceInFlight[_currentFrame]->getFence()};
  auto result = vkWaitForFences(_state->getDevice()->getLogicalDevice(), waitFences.size(), waitFences.data(), VK_TRUE,
                                UINT64_MAX);
  if (result != VK_SUCCESS) throw std::runtime_error("Can't wait for fence");

  result = vkResetFences(_state->getDevice()->getLogicalDevice(), waitFences.size(), waitFences.data());
  if (result != VK_SUCCESS) throw std::runtime_error("Can't reset fence");
  // RETURNS ONLY INDEX, NOT IMAGE
  // semaphore to signal, once image is available
  result = vkAcquireNextImageKHR(_state->getDevice()->getLogicalDevice(), _state->getSwapchain()->getSwapchain(),
                                 UINT64_MAX, _semaphoreImageAvailable[_currentFrame]->getSemaphore(), VK_NULL_HANDLE,
                                 imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    // TODO: recreate swapchain
    throw std::runtime_error("failed to acquire swap chain image!");
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }
}

void Core::_drawFrame(int imageIndex) {
  _currentFrame = _timer->getFrameCounter() % _state->getSettings()->getMaxFramesInFlight();

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

  VkSemaphore signalSemaphores[] = {_semaphorePostprocessing[_currentFrame]->getSemaphore()};
  std::vector<VkSemaphore> waitSemaphores = {_semaphoreParticleSystem[_currentFrame]->getSemaphore()};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT};
  auto particleSignal = _timer->getFrameCounter() + 1;
  std::vector<uint64_t> waitSemaphoreValues = {particleSignal};

  VkTimelineSemaphoreSubmitInfo waitParticlesSemaphore{};
  waitParticlesSemaphore.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
  waitParticlesSemaphore.pNext = NULL;
  waitParticlesSemaphore.waitSemaphoreValueCount = waitSemaphores.size();
  waitParticlesSemaphore.pWaitSemaphoreValues = waitSemaphoreValues.data();
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.pNext = &waitParticlesSemaphore;
  submitInfo.waitSemaphoreCount = waitSemaphores.size();
  submitInfo.pWaitSemaphores = waitSemaphores.data();
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &_commandBufferRender->getCommandBuffer()[_currentFrame];
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  _commandBufferRender->submitToQueue(submitInfo, nullptr);

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Render compute postprocessing
  //////////////////////////////////////////////////////////////////////////////////////////////////
  {
    if (postprocessingFuture.valid()) postprocessingFuture.get();

    VkSubmitInfo submitInfoPostprocessing{};
    submitInfoPostprocessing.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    std::vector<VkSemaphore> waitSemaphores = {_semaphoreImageAvailable[_currentFrame]->getSemaphore(),
                                               _semaphorePostprocessing[_currentFrame]->getSemaphore()};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
    submitInfoPostprocessing.waitSemaphoreCount = waitSemaphores.size();
    submitInfoPostprocessing.pWaitSemaphores = waitSemaphores.data();
    submitInfoPostprocessing.pWaitDstStageMask = waitStages;

    VkSemaphore signalComputeSemaphores[] = {_semaphoreGUI[_currentFrame]->getSemaphore()};
    submitInfoPostprocessing.signalSemaphoreCount = 1;
    submitInfoPostprocessing.pSignalSemaphores = signalComputeSemaphores;
    submitInfoPostprocessing.commandBufferCount = 1;
    submitInfoPostprocessing.pCommandBuffers = &_commandBufferPostprocessing->getCommandBuffer()[_currentFrame];

    // end command buffer
    _commandBufferPostprocessing->submitToQueue(submitInfoPostprocessing, nullptr);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Render debug visualization
  //////////////////////////////////////////////////////////////////////////////////////////////////
  {
    if (debugVisualizationFuture.valid()) debugVisualizationFuture.get();

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    std::vector<VkSemaphore> waitSemaphores = {_semaphoreGUI[_currentFrame]->getSemaphore()};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
    submitInfo.waitSemaphoreCount = waitSemaphores.size();
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_commandBufferGUI->getCommandBuffer()[_currentFrame];

    VkSemaphore signalSemaphores[] = {_semaphoreRenderFinished[_currentFrame]->getSemaphore()};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    // end command buffer
    // to render, need to wait semaphore from vkAcquireNextImageKHR, so display surface is ready
    // signal inFlightFences once all commands on GPU finish execution
    _commandBufferGUI->submitToQueue(submitInfo, _fenceInFlight[_currentFrame]);
  }
}

void Core::_displayFrame(uint32_t* imageIndex) {
  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  std::vector<VkSemaphore> waitSemaphoresPresent = {_semaphoreRenderFinished[_currentFrame]->getSemaphore()};

  presentInfo.waitSemaphoreCount = waitSemaphoresPresent.size();
  presentInfo.pWaitSemaphores = waitSemaphoresPresent.data();

  VkSwapchainKHR swapChains[] = {_state->getSwapchain()->getSwapchain()};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;

  presentInfo.pImageIndices = imageIndex;

  // TODO: change to own present queue
  auto result = vkQueuePresentKHR(_state->getDevice()->getQueue(QueueType::PRESENT), &presentInfo);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    // TODO: support window resize
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }
}

void Core::draw() {
  while (!glfwWindowShouldClose(_state->getWindow()->getWindow())) {
    glfwPollEvents();
    _timer->tick();
    _timerFPSReal->tick();
    _timerFPSLimited->tick();

    uint32_t imageIndex;
    _getImageIndex(&imageIndex);
    _drawFrame(imageIndex);
    _timerFPSReal->tock();
    // if GPU frames are limited by driver it will happen during display
    _displayFrame(&imageIndex);

    /*if (FPSChanged) {
      FPSChanged = false;
      _timer->reset();
    }*/

    _timer->sleep(_state->getSettings()->getDesiredFPS(), _loggerCPU);
    _timer->tock();
    _timerFPSLimited->tock();
  }
  vkDeviceWaitIdle(_state->getDevice()->getLogicalDevice());
}

std::shared_ptr<CommandBuffer> Core::getCommandBufferTransfer() { return _commandBufferTransfer; }

std::shared_ptr<LightManager> Core::getLightManager() { return _lightManager; }

std::shared_ptr<State> Core::getState() { return _state; }

void Core::addDrawable(std::shared_ptr<IDrawable> drawable) { _drawables.push_back(drawable); }

void Core::addShadowable(std::shared_ptr<IShadowable> shadowable) { _shadowables.push_back(shadowable); }

void Core::addParticleSystem(std::shared_ptr<ParticleSystem> particleSystem) {
  _particleSystem.push_back(particleSystem);
}