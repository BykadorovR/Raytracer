#include "Swapchain.h"
#include <algorithm>

Swapchain::Swapchain(VkFormat imageFormat, std::shared_ptr<State> state) {
  _state = state;
  _imageFormat = imageFormat;
  _initialize();
}

VkFormat& Swapchain::getImageFormat() { return _swapchainImageFormat; }

std::vector<std::shared_ptr<ImageView>>& Swapchain::getImageViews() { return _swapchainImageViews; }

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

Swapchain::~Swapchain() { _destroy(); }

void Swapchain::_initialize() {
  auto physicalDevice = _state->getDevice()->getPhysicalDevice();
  // pick surface format
  VkSurfaceFormatKHR surfaceFormat = _state->getDevice()->getSupportedSurfaceFormats()[0];
  for (const auto& availableFormat : _state->getDevice()->getSupportedSurfaceFormats()) {
    if (availableFormat.format == _imageFormat && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      surfaceFormat = availableFormat;
    }
  }
  // pick surface present mode
  VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
  for (const auto& availablePresentMode : _state->getDevice()->getSupportedSurfacePresentModes()) {
    // triple buffering
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      presentMode = availablePresentMode;
    }
  }

  // swap extent is the resolution of the swap chain images and
  // it's almost always exactly equal to the resolution of the window
  auto surfaceCapabilities = _state->getDevice()->getSupportedSurfaceCapabilities();
  VkExtent2D extent = surfaceCapabilities.currentExtent;
  if (surfaceCapabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
    int width, height;
#ifdef __ANDROID__
    std::tie(width, height) = _state->getWindow()->getResolution();
#else
    glfwGetFramebufferSize((GLFWwindow*)(_state->getWindow()->getWindow()), &width, &height);
#endif
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
  createInfo.surface = _state->getSurface()->getSurface();

  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

  uint32_t queueFamilyIndices[] = {_state->getDevice()->getSupportedFamilyIndex(QueueType::GRAPHIC).value(),
                                   _state->getDevice()->getSupportedFamilyIndex(QueueType::PRESENT).value()};
  // if we have separate queues for presentation and graphic we can use concurrent mode
  // render using graphic queue and show using presentation queue
  if (_state->getDevice()->getSupportedFamilyIndex(QueueType::GRAPHIC) !=
      _state->getDevice()->getSupportedFamilyIndex(QueueType::PRESENT)) {
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

  if (vkCreateSwapchainKHR(_state->getDevice()->getLogicalDevice(), &createInfo, nullptr, &_swapchain) != VK_SUCCESS) {
    throw std::runtime_error("failed to create swap chain!");
  }

  if (vkGetSwapchainImagesKHR(_state->getDevice()->getLogicalDevice(), _swapchain, &imageCount, nullptr) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create swap chain image!");
  }
  std::vector<VkImage> swapchainImages(imageCount);
  if (vkGetSwapchainImagesKHR(_state->getDevice()->getLogicalDevice(), _swapchain, &imageCount,
                              swapchainImages.data()) != VK_SUCCESS) {
    throw std::runtime_error("failed to create swap chain image!");
  }

  _swapchainImageFormat = surfaceFormat.format;
  _swapchainExtent = extent;

  _swapchainImageViews.resize(swapchainImages.size());
  for (uint32_t i = 0; i < swapchainImages.size(); i++) {
    auto image = std::make_shared<Image>(swapchainImages[i], std::tuple{extent.width, extent.height},
                                         surfaceFormat.format, _state);
    _swapchainImageViews[i] = std::make_shared<ImageView>(image, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                          VK_IMAGE_ASPECT_COLOR_BIT, _state);
  }
}

void Swapchain::_destroy() { vkDestroySwapchainKHR(_state->getDevice()->getLogicalDevice(), _swapchain, nullptr); }

void Swapchain::reset() {
  if (vkDeviceWaitIdle(_state->getDevice()->getLogicalDevice()) != VK_SUCCESS)
    throw std::runtime_error("failed to create reset swap chain!");

  _destroy();
  _initialize();
}