#include "Graphic/Shadow.h"

DirectionalShadow::DirectionalShadow(std::shared_ptr<CommandBuffer> commandBufferTransfer,
                                     std::shared_ptr<RenderPass> renderPass,
                                     std::shared_ptr<EngineState> engineState) {
  _engineState = engineState;
  // create shadow map texture
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    std::shared_ptr<Image> image = std::make_shared<Image>(
        engineState->getSettings()->getShadowMapResolution(), 1, 1, _engineState->getSettings()->getShadowMapFormat(),
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _engineState);
    image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                        commandBufferTransfer);
    auto imageView = std::make_shared<ImageView>(image, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT,
                                                 _engineState);
    // android doesn't support linear + d32 texture
    auto filter = VK_FILTER_NEAREST;
    if (_engineState->getDevice()->isFormatFeatureSupported(_engineState->getSettings()->getShadowMapFormat(),
                                                            VK_IMAGE_TILING_OPTIMAL,
                                                            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
      filter = VK_FILTER_LINEAR;
    }

    _shadowMapTexture.push_back(
        std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, filter, imageView, _engineState));
  }
  // create command buffer for rendering to shadow map
  auto commandPool = std::make_shared<CommandPool>(vkb::QueueType::graphics, _engineState->getDevice());
  _commandBufferDirectional = std::make_shared<CommandBuffer>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                              commandPool, _engineState);
  auto loggerUtils = std::make_shared<LoggerUtils>(_engineState);
  loggerUtils->setName("Command buffer directional ", VkObjectType::VK_OBJECT_TYPE_COMMAND_BUFFER,
                       _commandBufferDirectional->getCommandBuffer());
  _loggerDirectional = std::make_shared<Logger>(_engineState);
  // create framebuffer to render shadow map to
  _shadowMapFramebuffer.resize(_engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _shadowMapFramebuffer[i] = std::make_shared<Framebuffer>(
        std::vector{_shadowMapTexture[i]->getImageView()},
        _shadowMapTexture[i]->getImageView()->getImage()->getResolution(), renderPass, _engineState->getDevice());
  }
}
std::shared_ptr<CommandBuffer> DirectionalShadow::getShadowMapCommandBuffer() { return _commandBufferDirectional; }

std::shared_ptr<Logger> DirectionalShadow::getShadowMapLogger() { return _loggerDirectional; }

std::vector<std::shared_ptr<Texture>> DirectionalShadow::getShadowMapTexture() { return _shadowMapTexture; }

std::vector<std::shared_ptr<Framebuffer>> DirectionalShadow::getShadowMapFramebuffer() { return _shadowMapFramebuffer; }

PointShadow::PointShadow(std::shared_ptr<CommandBuffer> commandBufferTransfer,
                         std::shared_ptr<RenderPass> renderPass,
                         std::shared_ptr<EngineState> engineState) {
  _engineState = engineState;
  // create shadow map cubemap texture
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    auto filter = VK_FILTER_NEAREST;
    if (_engineState->getDevice()->isFormatFeatureSupported(_engineState->getSettings()->getShadowMapFormat(),
                                                            VK_IMAGE_TILING_OPTIMAL,
                                                            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
      filter = VK_FILTER_LINEAR;
    }
    _shadowMapCubemap.push_back(std::make_shared<Cubemap>(
        engineState->getSettings()->getShadowMapResolution(), _engineState->getSettings()->getShadowMapFormat(), 1,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, filter, commandBufferTransfer, _engineState));
  }
  // should be unique command pool for every command buffer to work in parallel.
  // the same with logger, it's binded to command buffer (//TODO: maybe fix somehow)
  auto loggerUtils = std::make_shared<LoggerUtils>(_engineState);
  _commandBufferPoint.resize(6);
  _loggerPoint.resize(6);
  for (int j = 0; j < _commandBufferPoint.size(); j++) {
    auto commandPool = std::make_shared<CommandPool>(vkb::QueueType::graphics, _engineState->getDevice());
    _commandBufferPoint[j] = std::make_shared<CommandBuffer>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                             commandPool, _engineState);
    loggerUtils->setName("Command buffer point " + std::to_string(j), VkObjectType::VK_OBJECT_TYPE_COMMAND_BUFFER,
                         _commandBufferPoint[j]->getCommandBuffer());
    _loggerPoint[j] = std::make_shared<Logger>(_engineState);
  };
  // create framebuffer to render shadow map to
  _shadowMapFramebuffer.resize(_engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    std::vector<std::shared_ptr<Framebuffer>> pointLightFaces(6);
    for (int j = 0; j < 6; j++) {
      auto imageView = _shadowMapCubemap[i]->getTextureSeparate()[j][0]->getImageView();
      pointLightFaces[j] = std::make_shared<Framebuffer>(std::vector{imageView}, imageView->getImage()->getResolution(),
                                                         renderPass, _engineState->getDevice());
    }
    _shadowMapFramebuffer[i] = pointLightFaces;
  }
}

std::vector<std::shared_ptr<CommandBuffer>> PointShadow::getShadowMapCommandBuffer() { return _commandBufferPoint; }

std::vector<std::shared_ptr<Logger>> PointShadow::getShadowMapLogger() { return _loggerPoint; }

std::vector<std::shared_ptr<Cubemap>> PointShadow::getShadowMapCubemap() { return _shadowMapCubemap; }

std::vector<std::vector<std::shared_ptr<Framebuffer>>> PointShadow::getShadowMapFramebuffer() {
  return _shadowMapFramebuffer;
}

DirectionalShadowBlur::DirectionalShadowBlur(std::vector<std::shared_ptr<Texture>> textureIn,
                                             std::shared_ptr<CommandBuffer> commandBufferTransfer,
                                             std::shared_ptr<RenderPass> renderPass,
                                             std::shared_ptr<EngineState> engineState) {
  auto resolution = engineState->getSettings()->getShadowMapResolution();
  // create texture, frame buffers and blurs for directional lights shadow maps postprocessing
  std::vector<std::shared_ptr<Framebuffer>> blurLightOut(engineState->getSettings()->getMaxFramesInFlight());
  std::vector<std::shared_ptr<Framebuffer>> blurLightIn(engineState->getSettings()->getMaxFramesInFlight());
  _textureOut.resize(engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < engineState->getSettings()->getMaxFramesInFlight(); i++) {
    auto blurImage = std::make_shared<Image>(resolution, 1, 1, engineState->getSettings()->getShadowMapFormat(),
                                             VK_IMAGE_TILING_OPTIMAL,
                                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, engineState);
    blurImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                            commandBufferTransfer);
    auto blurImageView = std::make_shared<ImageView>(blurImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                     VK_IMAGE_ASPECT_COLOR_BIT, engineState);
    auto filter = VK_FILTER_NEAREST;
    if (engineState->getDevice()->isFormatFeatureSupported(engineState->getSettings()->getShadowMapFormat(),
                                                           VK_IMAGE_TILING_OPTIMAL,
                                                           VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
      filter = VK_FILTER_LINEAR;
    }
    _textureOut[i] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, filter, blurImageView,
                                               engineState);
    blurLightIn[i] = std::make_shared<Framebuffer>(std::vector{_textureOut[i]->getImageView()}, resolution, renderPass,
                                                   engineState->getDevice());
    blurLightOut[i] = std::make_shared<Framebuffer>(std::vector{textureIn[i]->getImageView()}, resolution, renderPass,
                                                    engineState->getDevice());
  }
  _shadowMapFramebuffer = {blurLightIn, blurLightOut};

  _blur = std::make_shared<BlurGraphic>(textureIn, _textureOut, commandBufferTransfer, engineState);
  // create buffer pool and command buffer
  auto commandPool = std::make_shared<CommandPool>(vkb::QueueType::graphics, engineState->getDevice());
  _commandBufferDirectional = std::make_shared<CommandBuffer>(engineState->getSettings()->getMaxFramesInFlight(),
                                                              commandPool, engineState);
  auto loggerUtils = std::make_shared<LoggerUtils>(engineState);
  loggerUtils->setName("Command buffer blur directional", VkObjectType::VK_OBJECT_TYPE_COMMAND_BUFFER,
                       _commandBufferDirectional->getCommandBuffer());
  _loggerDirectional = std::make_shared<Logger>(engineState);
}

std::shared_ptr<CommandBuffer> DirectionalShadowBlur::getShadowMapBlurCommandBuffer() {
  return _commandBufferDirectional;
}

std::shared_ptr<Logger> DirectionalShadowBlur::getShadowMapBlurLogger() { return _loggerDirectional; }

std::vector<std::vector<std::shared_ptr<Framebuffer>>> DirectionalShadowBlur::getShadowMapBlurFramebuffer() {
  return _shadowMapFramebuffer;
}

std::shared_ptr<BlurGraphic> DirectionalShadowBlur::getBlur() { return _blur; }

std::vector<std::shared_ptr<Texture>> DirectionalShadowBlur::getShadowMapBlurTextureOut() { return _textureOut; }

PointShadowBlur::PointShadowBlur(std::vector<std::shared_ptr<Cubemap>> cubemapIn,
                                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                                 std::shared_ptr<RenderPass> renderPass,
                                 std::shared_ptr<EngineState> engineState) {
  auto resolution = engineState->getSettings()->getShadowMapResolution();
  // create texture, frame buffers and blurs for point lights shadow maps postprocessing
  std::vector<std::vector<std::shared_ptr<Framebuffer>>> blurLightOut(
      engineState->getSettings()->getMaxFramesInFlight());
  std::vector<std::vector<std::shared_ptr<Framebuffer>>> blurLightIn(
      engineState->getSettings()->getMaxFramesInFlight());
  _cubemapOut.resize(engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < engineState->getSettings()->getMaxFramesInFlight(); i++) {
    std::vector<std::shared_ptr<Framebuffer>> blurLightOutFaces(6);
    std::vector<std::shared_ptr<Framebuffer>> blurLightInFaces(6);
    std::shared_ptr<Cubemap> cubemapBlurOutFaces;
    auto filter = VK_FILTER_NEAREST;
    if (engineState->getDevice()->isFormatFeatureSupported(engineState->getSettings()->getShadowMapFormat(),
                                                           VK_IMAGE_TILING_OPTIMAL,
                                                           VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
      filter = VK_FILTER_LINEAR;
    }

    cubemapBlurOutFaces = std::make_shared<Cubemap>(resolution, engineState->getSettings()->getShadowMapFormat(), 1,
                                                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT,
                                                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                                    filter, commandBufferTransfer, engineState);
    for (int j = 0; j < 6; j++) {
      blurLightInFaces[j] = std::make_shared<Framebuffer>(
          std::vector{cubemapBlurOutFaces->getTextureSeparate()[j][0]->getImageView()}, resolution, renderPass,
          engineState->getDevice());
      blurLightOutFaces[j] = std::make_shared<Framebuffer>(
          std::vector{cubemapIn[i]->getTextureSeparate()[j][0]->getImageView()}, resolution, renderPass,
          engineState->getDevice());
    }
    blurLightOut[i] = blurLightOutFaces;
    blurLightIn[i] = blurLightInFaces;
    _cubemapOut[i] = cubemapBlurOutFaces;
  }

  _shadowMapFramebuffer = {blurLightIn, blurLightOut};

  for (int j = 0; j < 6; j++) {
    std::vector<std::shared_ptr<Texture>> texturesIn, texturesOut;
    for (int i = 0; i < engineState->getSettings()->getMaxFramesInFlight(); i++) {
      texturesIn.push_back(cubemapIn[i]->getTextureSeparate()[j][0]);
      texturesOut.push_back(_cubemapOut[i]->getTextureSeparate()[j][0]);
    }
    _blur.push_back(std::make_shared<BlurGraphic>(texturesIn, texturesOut, commandBufferTransfer, engineState));
  }

  // create buffer pool and command buffer
  auto loggerUtils = std::make_shared<LoggerUtils>(engineState);
  _commandBufferPoint.resize(6);
  _loggerPoint.resize(6);
  for (int i = 0; i < 6; i++) {
    auto commandPool = std::make_shared<CommandPool>(vkb::QueueType::graphics, engineState->getDevice());
    auto commandBuffer = std::make_shared<CommandBuffer>(engineState->getSettings()->getMaxFramesInFlight(),
                                                         commandPool, engineState);
    loggerUtils->setName("Command buffer blur point ", VkObjectType::VK_OBJECT_TYPE_COMMAND_BUFFER,
                         commandBuffer->getCommandBuffer());
    _commandBufferPoint[i] = commandBuffer;

    _loggerPoint[i] = std::make_shared<Logger>(engineState);
  }
}

std::vector<std::shared_ptr<CommandBuffer>> PointShadowBlur::getShadowMapBlurCommandBuffer() {
  return _commandBufferPoint;
}

std::vector<std::shared_ptr<Logger>> PointShadowBlur::getShadowMapBlurLogger() { return _loggerPoint; }

std::vector<std::vector<std::vector<std::shared_ptr<Framebuffer>>>> PointShadowBlur::getShadowMapBlurFramebuffer() {
  return _shadowMapFramebuffer;
}

std::vector<std::shared_ptr<BlurGraphic>> PointShadowBlur::getBlur() { return _blur; }

std::vector<std::shared_ptr<Cubemap>> PointShadowBlur::getShadowMapBlurCubemapOut() { return _cubemapOut; }