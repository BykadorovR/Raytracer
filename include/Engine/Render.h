#pragma once
#include "Swapchain.h"

class RenderPass {
 private:
  std::shared_ptr<Device> _device;
  VkRenderPass _renderPass;
  VkFormat _format;

 public:
  RenderPass(VkFormat format, std::shared_ptr<Device> device);
  void initialize();
  void initializeOffscreen();
  void initializeDepthPass();
  VkRenderPass& getRenderPass();
  ~RenderPass();
};

class Framebuffer {
 private:
  std::shared_ptr<Device> _device;
  std::vector<VkFramebuffer> _buffer;

 public:
  Framebuffer(std::tuple<int, int> resolution,
              std::vector<std::shared_ptr<ImageView>> imageViews,
              std::shared_ptr<ImageView> depthImageView,
              std::shared_ptr<RenderPass> renderPass,
              std::shared_ptr<Device> device);

  // for depth buffer generation pass
  Framebuffer(std::tuple<int, int> resolution,
              std::vector<std::shared_ptr<ImageView>> depthImageViews,
              std::shared_ptr<RenderPass> renderPass,
              std::shared_ptr<Device> device);

  std::vector<VkFramebuffer>& getBuffer();
  ~Framebuffer();
};
