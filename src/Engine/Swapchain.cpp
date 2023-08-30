#include "Swapchain.h"
#include <algorithm>

Swapchain::Swapchain(VkFormat imageFormat,
                     VkFormat depthFormat,
                     std::shared_ptr<Window> window,
                     std::shared_ptr<Surface> surface,
                     std::shared_ptr<Device> device) {
  _device = device;

  auto physicalDevice = device->getPhysicalDevice();
  // pick surface format
  VkSurfaceFormatKHR surfaceFormat = device->getSupportedSurfaceFormats()[0];
  for (const auto& availableFormat : device->getSupportedSurfaceFormats()) {
    if (availableFormat.format == imageFormat && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      surfaceFormat = availableFormat;
    }
  }
  // pick surface present mode
  VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
  for (const auto& availablePresentMode : device->getSupportedSurfacePresentModes()) {
    // triple buffering
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      presentMode = availablePresentMode;
    }
  }

  // swap extent is the resolution of the swap chain images and
  // it's almost always exactly equal to the resolution of the window
  auto surfaceCapabilities = device->getSupportedSurfaceCapabilities();
  VkExtent2D extent = surfaceCapabilities.currentExtent;
  if (surfaceCapabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
    int width, height;
    glfwGetFramebufferSize(window->getWindow(), &width, &height);

    VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

    actualExtent.width = std::clamp(actualExtent.width, surfaceCapabilities.minImageExtent.width,
                                    surfaceCapabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, surfaceCapabilities.minImageExtent.height,
                                     surfaceCapabilities.maxImageExtent.height);
    extent = actualExtent;
  }

  // pick how many images we will have in swap chaing
  uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
  if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount) {
    imageCount = surfaceCapabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = surface->getSurface();

  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

  uint32_t queueFamilyIndices[] = {device->getSupportedFamilyIndex(QueueType::GRAPHIC).value(),
                                   device->getSupportedFamilyIndex(QueueType::PRESENT).value()};
  // if we have separate queues for presentation and graphic we can use concurrent mode
  // render using graphic queue and show using presentation queue
  if (device->getSupportedFamilyIndex(QueueType::GRAPHIC) != device->getSupportedFamilyIndex(QueueType::PRESENT)) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  createInfo.preTransform = surfaceCapabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;

  if (vkCreateSwapchainKHR(device->getLogicalDevice(), &createInfo, nullptr, &_swapchain) != VK_SUCCESS) {
    throw std::runtime_error("failed to create swap chain!");
  }

  vkGetSwapchainImagesKHR(device->getLogicalDevice(), _swapchain, &imageCount, nullptr);
  _swapchainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(device->getLogicalDevice(), _swapchain, &imageCount, _swapchainImages.data());

  _swapchainImageFormat = surfaceFormat.format;
  _swapchainExtent = extent;

  _swapchainImageViews.resize(_swapchainImages.size());

  for (uint32_t i = 0; i < _swapchainImages.size(); i++) {
    auto image = std::make_shared<Image>(_swapchainImages[i], std::tuple{extent.width, extent.height},
                                         surfaceFormat.format, device);
    auto imageView = std::make_shared<ImageView>(image, VK_IMAGE_VIEW_TYPE_2D, 1, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT,
                                                 device);

    _swapchainImageViews[i] = imageView;
  }

  _depthImage = std::make_shared<Image>(std::tuple{extent.width, extent.height}, 1, 1, depthFormat,
                                        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, device);
  _depthImageView = std::make_shared<ImageView>(_depthImage, VK_IMAGE_VIEW_TYPE_2D, 1, 0, 1, VK_IMAGE_ASPECT_DEPTH_BIT,
                                                device);
}

VkFormat& Swapchain::getImageFormat() { return _swapchainImageFormat; }

std::vector<std::shared_ptr<ImageView>>& Swapchain::getImageViews() { return _swapchainImageViews; }

std::shared_ptr<ImageView> Swapchain::getDepthImageView() { return _depthImageView; }

void Swapchain::changeImageLayout(VkImageLayout imageLayout, std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  for (int i = 0; i < _swapchainImageViews.size(); i++) {
    _swapchainImageViews[i]->getImage()->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, imageLayout, VK_IMAGE_ASPECT_COLOR_BIT,
                                                      1, 1, commandBufferTransfer);
  }
}

void Swapchain::overrideImageLayout(VkImageLayout imageLayout) {
  for (int i = 0; i < _swapchainImageViews.size(); i++) {
    _swapchainImageViews[i]->getImage()->overrideLayout(VK_IMAGE_LAYOUT_GENERAL);
  }
}

VkSwapchainKHR& Swapchain::getSwapchain() { return _swapchain; }

VkExtent2D& Swapchain::getSwapchainExtent() { return _swapchainExtent; }

Swapchain::~Swapchain() { vkDestroySwapchainKHR(_device->getLogicalDevice(), _swapchain, nullptr); }