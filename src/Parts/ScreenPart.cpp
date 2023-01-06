#include "ScreenPart.h"

ScreenPart::ScreenPart(std::vector<std::shared_ptr<Texture>> resultTexture,
                       std::shared_ptr<Window> window,
                       std::shared_ptr<Surface> surface,
                       std::shared_ptr<Device> device,
                       std::shared_ptr<Queue> queue,
                       std::shared_ptr<CommandPool> commandPool,
                       std::shared_ptr<CommandBuffer> commandBuffer,
                       std::shared_ptr<Settings> settings) {
  _swapchain = std::make_shared<Swapchain>(window, surface, device);
  _renderPass = std::make_shared<RenderPass>(_swapchain->getImageFormat(), device);
  _renderPass->initialize();

  _frameBuffer = std::make_shared<Framebuffer>(settings->getResolution(), _swapchain->getImageViews(),
                                               _swapchain->getDepthImageView(), _renderPass, device);

  auto shaderGray = std::make_shared<Shader>(device);
  shaderGray->add("../shaders/final_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shaderGray->add("../shaders/final_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  _spriteManager = std::make_shared<SpriteManager>(shaderGray, commandPool, commandBuffer, queue, _renderPass, device,
                                                   settings);
  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    _sprites.push_back(_spriteManager->createSprite(resultTexture[i]));
  }
}

std::shared_ptr<RenderPass> ScreenPart::getRenderPass() { return _renderPass; }
std::shared_ptr<Framebuffer> ScreenPart::getFramebuffer() { return _frameBuffer; }
std::vector<std::shared_ptr<Sprite>> ScreenPart::getSprites() { return _sprites; }
std::shared_ptr<SpriteManager> ScreenPart::getSpriteManager() { return _spriteManager; }
std::shared_ptr<Swapchain> ScreenPart::getSwapchain() { return _swapchain; }