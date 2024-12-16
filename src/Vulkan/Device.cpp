#include "Vulkan/Device.h"

Device::Device(std::shared_ptr<Surface> surface, std::shared_ptr<Instance> instance) {
  VkPhysicalDeviceFeatures deviceFeatures{
      .geometryShader = VK_TRUE,
      .tessellationShader = VK_TRUE,
      .fillModeNonSolid = VK_TRUE,
      .samplerAnisotropy = VK_TRUE,
  };

  vkb::PhysicalDeviceSelector deviceSelector(instance->getInstance());
  deviceSelector.set_required_features(deviceFeatures);

  // VK_KHR_SWAPCHAIN_EXTENSION_NAME is added by default
  auto deviceSelectorResult = deviceSelector.set_surface(surface->getSurface()).select();
  if (!deviceSelectorResult) {
    throw std::runtime_error(deviceSelectorResult.error().message());
  }
  auto devicePhysical = deviceSelectorResult.value();

  vkb::DeviceBuilder builder{devicePhysical};
  auto builderResult = builder.build();
  if (!builderResult) {
    throw std::runtime_error(builderResult.error().message());
  }
  _device = builderResult.value();
  volkLoadDevice(_device.device);

  VkPhysicalDeviceProperties props;
  vkGetPhysicalDeviceProperties(getPhysicalDevice(), &props);
  _deviceLimits = props.limits;
}

VkPhysicalDeviceLimits& Device::getDeviceLimits() { return _deviceLimits; }

bool Device::isFormatFeatureSupported(VkFormat format, VkImageTiling tiling, VkFormatFeatureFlagBits featureFlagBit) {
  VkFormatProperties props;
  vkGetPhysicalDeviceFormatProperties(getPhysicalDevice(), format, &props);

  if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & featureFlagBit) == featureFlagBit) {
    return true;
  } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & featureFlagBit) == featureFlagBit) {
    return true;
  }

  return false;
}

const VkDevice& Device::getLogicalDevice() { return _device.device; }

const VkPhysicalDevice& Device::getPhysicalDevice() { return _device.physical_device; }

const vkb::Device& Device::getDevice() { return _device; }

VkQueue Device::getQueue(vkb::QueueType type) {
  auto queueResult = _device.get_dedicated_queue(type);
  if (!queueResult) {
    queueResult = _device.get_queue(type);
    // use default queue that should support everything
    if (!queueResult) {
      queueResult = _device.get_queue(vkb::QueueType::present);
    }
  }

  return queueResult.value();
}

int Device::getQueueIndex(vkb::QueueType type) {
  auto queueResult = _device.get_dedicated_queue_index(type);
  if (!queueResult) {
    queueResult = _device.get_queue_index(type);
    // use default queue that should support everything
    if (!queueResult) {
      queueResult = _device.get_queue_index(vkb::QueueType::present);
    }
  }

  return queueResult.value();
}

Device::~Device() { vkb::destroy_device(_device); }