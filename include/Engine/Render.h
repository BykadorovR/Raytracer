#pragma once
#include "Swapchain.h"

class RenderPass {
 private:
  std::shared_ptr<Device> _device;
  VkRenderPass _renderPass;

 public:
  RenderPass(std::shared_ptr<Device> device);
  void initialize(VkFormat format);
  void initializeOffscreen(VkFormat format);
  void initializeDepthPass();
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
