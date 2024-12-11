#pragma once
#include <vector>
#include <string>
#include <exception>
#include <stdexcept>
#include <iostream>
#include <optional>
#include <set>
#include <map>
#include <mutex>
#include <memory>

#include "Vulkan/Instance.h"
#include "Vulkan/Surface.h"

class Device {
 private:
  vkb::Device _device;
  VkPhysicalDeviceLimits _deviceLimits;

 public:
  Device(std::shared_ptr<Surface> surface, std::shared_ptr<Instance> instance);
  bool isFormatFeatureSupported(VkFormat format, VkImageTiling tiling, VkFormatFeatureFlagBits featureFlagBit);
  const VkDevice& getLogicalDevice();
  const VkPhysicalDevice& getPhysicalDevice();
  const vkb::Device& getDevice();
  VkQueue getQueue(vkb::QueueType type);
  int getQueueIndex(vkb::QueueType type);

  VkPhysicalDeviceLimits& getDeviceLimits();

  ~Device();
};