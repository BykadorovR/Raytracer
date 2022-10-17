#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <exception>
#include <stdexcept>
#include <iostream>
#include <optional>
#include <set>

#include "Instance.h"
#include "Surface.h"

class Device {
 private:
  std::shared_ptr<Instance> _instance;
  std::shared_ptr<Surface> _surface;
  VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
  VkDevice _logicalDevice;
  // device extension
  const std::vector<const char*> _deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  // supported device features
  VkPhysicalDeviceFeatures _supportedFeatures;
  // supported queues
  std::optional<uint32_t> _graphicsFamily;
  std::optional<uint32_t> _presentFamily;
  // supported surface capabilities
  VkSurfaceCapabilitiesKHR _surfaceCapabilities;
  std::vector<VkSurfaceFormatKHR> _surfaceFormats;
  std::vector<VkPresentModeKHR> _surfacePresentModes;

  void _createLogicalDevice();
  void _pickPhysicalDevice();
  bool _isDeviceSuitable(VkPhysicalDevice device);

 public:
  Device(std::shared_ptr<Surface> surface, std::shared_ptr<Instance> instance);
  VkDevice& getLogicalDevice();
  VkPhysicalDevice& getPhysicalDevice();
  std::vector<VkSurfaceFormatKHR>& getSupportedSurfaceFormats();
  std::vector<VkPresentModeKHR>& getSupportedSurfacePresentModes();
  VkSurfaceCapabilitiesKHR& getSupportedSurfaceCapabilities();
  std::optional<uint32_t> getSupportedGraphicsFamilyIndex();
  std::optional<uint32_t> getSupportedPresentFamilyIndex();
  ~Device();
};