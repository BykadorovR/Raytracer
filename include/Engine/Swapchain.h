#pragma once
#include "Device.h"
#include "Image.h"

class Swapchain {
 private:
  std::shared_ptr<Device> _device;
  VkSwapchainKHR _swapchain;
  std::vector<VkImage> _swapchainImages;
  std::vector<std::shared_ptr<ImageView>> _swapchainImageViews;
  std::shared_ptr<Image> _depthImage;
  std::shared_ptr<ImageView> _depthImageView;
  VkFormat _swapchainImageFormat;
  VkExtent2D _swapchainExtent;

 public:
  Swapchain(VkFormat imageFormat,
            VkFormat depthFormat,
            std::shared_ptr<Window> window,
            std::shared_ptr<Surface> surface,
            std::shared_ptr<Device> device);
  void changeImageLayout(VkImageLayout imageLayout, std::shared_ptr<CommandBuffer> commandBufferTransfer);
  VkSwapchainKHR& getSwapchain();
  VkExtent2D& getSwapchainExtent();
  std::vector<std::shared_ptr<ImageView>>& getImageViews();
  std::shared_ptr<ImageView> getDepthImageView();
  VkFormat& getImageFormat();
  ~Swapchain();
};