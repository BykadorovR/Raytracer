#pragma once
#include "Swapchain.h"

class Framebuffer;

class RenderPass {
 private:
  std::shared_ptr<Device> _device;
  std::shared_ptr<Settings> _settings;
  VkRenderPass _renderPass;

 public:
  RenderPass(std::shared_ptr<Settings> settings, std::shared_ptr<Device> device);
  void initializeGraphic();
  void initializeDebug();
  void initializeLightDepth();
  VkRenderPassBeginInfo getRenderPassInfo(int index, std::shared_ptr<Framebuffer> framebuffer);
  VkRenderPass& getRenderPass();
  ~RenderPass();
};

class Framebuffer {
 private:
  std::shared_ptr<Device> _device;
  std::vector<VkFramebuffer> _buffer;
  std::tuple<int, int> _resolution;

 public:
  Framebuffer(std::vector<std::shared_ptr<ImageView>> imageViews,
              std::shared_ptr<ImageView> depthImageView,
              std::shared_ptr<RenderPass> renderPass,
              std::shared_ptr<Device> device);

  // for depth buffer generation pass
  Framebuffer(std::vector<std::shared_ptr<ImageView>> depthImageViews,
              std::shared_ptr<RenderPass> renderPass,
              std::shared_ptr<Device> device);

  std::tuple<int, int> getResolution();
  std::vector<VkFramebuffer>& getBuffer();
  ~Framebuffer();
};
