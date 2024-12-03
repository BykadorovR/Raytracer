#include "Graphic/Shadow.h"

DirectionalShadow::DirectionalShadow(std::shared_ptr<CommandBuffer> commandBufferTransfer,
                                     std::shared_ptr<RenderPass> renderPass,
                                     std::shared_ptr<DebuggerUtils> debuggerUtils,
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
#ifdef __ANDROID__
    _shadowMapTexture.push_back(std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, VK_FILTER_NEAREST,
                                                          imageView, _engineState));
#else
    _shadowMapTexture.push_back(std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, VK_FILTER_LINEAR,
                                                          imageView, _engineState));
#endif
  }
  // create command buffer for rendering to shadow map
  auto commandPool = std::make_shared<CommandPool>(QueueType::GRAPHIC, _engineState->getDevice());
  _commandBufferDirectional = std::make_shared<CommandBuffer>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                              commandPool, _engineState);
  debuggerUtils->setName("Command buffer directional ", VkObjectType::VK_OBJECT_TYPE_COMMAND_BUFFER,
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
                         std::shared_ptr<DebuggerUtils> debuggerUtils,
                         std::shared_ptr<EngineState> engineState) {
  _engineState = engineState;
  // create shadow map cubemap texture
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
#ifdef __ANDROID__
    _shadowMapCubemap.push_back(std::make_shared<Cubemap>(
        engineState->getSettings()->getShadowMapResolution(), _engineState->getSettings()->getShadowFormat(), 1,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FILTER_NEAREST, commandBufferTransfer,
        _engineState));
#else
    _shadowMapCubemap.push_back(std::make_shared<Cubemap>(
        engineState->getSettings()->getShadowMapResolution(), _engineState->getSettings()->getShadowMapFormat(), 1,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FILTER_LINEAR, commandBufferTransfer,
        _engineState));
#endif
  }
  // should be unique command pool for every command buffer to work in parallel.
  // the same with logger, it's binded to command buffer (//TODO: maybe fix somehow)
  _commandBufferPoint.resize(6);
  _loggerPoint.resize(6);
  for (int j = 0; j < _commandBufferPoint.size(); j++) {
    auto commandPool = std::make_shared<CommandPool>(QueueType::GRAPHIC, _engineState->getDevice());
    _commandBufferPoint[j] = std::make_shared<CommandBuffer>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                             commandPool, _engineState);
    debuggerUtils->setName("Command buffer point " + std::to_string(j), VkObjectType::VK_OBJECT_TYPE_COMMAND_BUFFER,
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