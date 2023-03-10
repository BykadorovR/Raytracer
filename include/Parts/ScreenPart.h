#pragma once
#include "Device.h"
#include "Render.h"
#include "Texture.h"
#include "Settings.h"
#include "Shader.h"
#include "SpriteManager.h"

class ScreenPart {
 private:
  std::shared_ptr<Device> _device;
  std::shared_ptr<Queue> _queue;
  std::shared_ptr<CommandPool> _commandPool;
  std::shared_ptr<CommandBuffer> _commandBuffer;
  std::shared_ptr<Settings> _settings;

  std::shared_ptr<Swapchain> _swapchain;
  std::shared_ptr<RenderPass> _renderPass;
  std::shared_ptr<Framebuffer> _frameBuffer;
  std::shared_ptr<SpriteManager> _spriteManager;
  std::vector<std::shared_ptr<Sprite>> _sprites;

 public:
  ScreenPart(std::vector<std::shared_ptr<Texture>> resultTexture,
             std::shared_ptr<Window> window,
             std::shared_ptr<Surface> surface,
             std::shared_ptr<Device> device,
             std::shared_ptr<Queue> queue,
             std::shared_ptr<CommandPool> commandPool,
             std::shared_ptr<CommandBuffer> commandBuffer,
             std::shared_ptr<Settings> settings);

  void recreateSwapChain(std::shared_ptr<Window> window,
                         std::shared_ptr<Surface> surface,
                         std::shared_ptr<Device> device,
                         std::shared_ptr<Settings> settings);
  std::shared_ptr<Framebuffer> getFramebuffer();
  std::shared_ptr<RenderPass> getRenderPass();
  std::shared_ptr<Swapchain> getSwapchain();
  std::vector<std::shared_ptr<Sprite>> getSprites();
  std::shared_ptr<SpriteManager> getSpriteManager();
};