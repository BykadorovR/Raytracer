#pragma once
#include "Device.h"
#include "Render.h"
#include "Texture.h"
#include "Settings.h"
#include "Shader.h"

class OffscreenPart {
 private:
  std::shared_ptr<Device> _device;
  std::shared_ptr<Queue> _queue;
  std::shared_ptr<CommandPool> _commandPool;
  std::shared_ptr<CommandBuffer> _commandBuffer;
  std::shared_ptr<Settings> _settings;

  std::shared_ptr<RenderPass> _renderPass;
  std::shared_ptr<Framebuffer> _framebuffer;

  std::vector<std::shared_ptr<Texture>> _resultTextures;
  std::vector<std::shared_ptr<ImageView>> _resultImageViews;
  std::shared_ptr<ImageView> _resultDepth;

 public:
  OffscreenPart(std::shared_ptr<Device> device,
                std::shared_ptr<Queue> queue,
                std::shared_ptr<CommandPool> commandPool,
                std::shared_ptr<CommandBuffer> commandBuffer,
                std::shared_ptr<Settings> settings);
  std::shared_ptr<Framebuffer> getFramebuffer();
  std::shared_ptr<RenderPass> getRenderPass();
  std::vector<std::shared_ptr<Texture>> getResultTextures();
};