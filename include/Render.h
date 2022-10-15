#pragma once
#include "Swapchain.h"

class RenderPass {
 private:
  std::shared_ptr<Device> _device;
  VkRenderPass _renderPass;

  VkFormat _findSupportedFormat(const std::vector<VkFormat>& candidates,
                                VkImageTiling tiling,
                                VkFormatFeatureFlags features);

 public:
  RenderPass(std::shared_ptr<Swapchain> swapchain, std::shared_ptr<Device> device);
  VkRenderPass& getRenderPass();
  ~RenderPass();
};

class Framebuffer {
 private:
  std::shared_ptr<Device> _device;
  std::vector<VkFramebuffer> _buffer;

 public:
  Framebuffer(std::tuple<int, int> resolution,
              std::shared_ptr<Swapchain> swapchain,
              std::shared_ptr<RenderPass> renderPass,
              std::shared_ptr<Device> device);
  std::vector<VkFramebuffer>& getBuffer();
  ~Framebuffer();
};
