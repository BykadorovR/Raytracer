#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <exception>
#include <stdexcept>
#include <iostream>
#include <optional>
#include <set>
#include <map>
#include <mutex>

#include "Instance.h"
#include "Surface.h"

enum class QueueType { PRESENT = 0, GRAPHIC, COMPUTE, TRANSFER };

class Device {
 private:
  std::shared_ptr<Instance> _instance;
  std::shared_ptr<Surface> _surface;
  VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
  VkDevice _logicalDevice;
  // device extension
  const std::vector<const char*> _deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, VK_EXT_ROBUSTNESS_2_EXTENSION_NAME};
  // supported device features
  VkPhysicalDeviceFeatures _supportedFeatures;
  // supported queues
  std::map<QueueType, std::optional<uint32_t>> _family;
  std::map<std::tuple<QueueType, int>, std::mutex> _queueMutex;
  std::mutex _mapMutex;
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
  std::optional<uint32_t> getSupportedFamilyIndex(QueueType type);
  VkQueue getQueue(QueueType type, int index = 0);
  std::mutex& getQueueMutex(QueueType type, int index = 0);

  VkFormat findDepthBufferSupportedFormat(const std::vector<VkFormat>& candidates,
                                          VkImageTiling tiling,
                                          VkFormatFeatureFlags features);
  ~Device();
};