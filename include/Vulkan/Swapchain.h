#pragma once
#include "Vulkan/Device.h"
#include "Vulkan/Image.h"

class Swapchain {
 private:
  std::shared_ptr<EngineState> _engineState;

  VkSwapchainKHR _swapchain;
  std::vector<std::shared_ptr<ImageView>> _swapchainImageViews;
  VkFormat _swapchainImageFormat;
  VkExtent2D _swapchainExtent;
  VkFormat _imageFormat;
  VkFormat _depthFormat;

  void _initialize();
  void _destroy();

 public:
  Swapchain(VkFormat imageFormat, std::shared_ptr<EngineState> engineState);
  void reset();

  void overrideImageLayout(VkImageLayout imageLayout);
  void changeImageLayout(VkImageLayout imageLayout, std::shared_ptr<CommandBuffer> commandBufferTransfer);
  VkSwapchainKHR& getSwapchain();
  VkExtent2D& getSwapchainExtent();
  std::vector<std::shared_ptr<ImageView>>& getImageViews();
  VkFormat& getImageFormat();
  ~Swapchain();
};