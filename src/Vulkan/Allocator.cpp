// vma doesn't have a build target as stb, so need to include implementation
#define VMA_IMPLEMENTATION
#include "Vulkan/Allocator.h"

MemoryAllocator::MemoryAllocator(std::shared_ptr<Device> device, std::shared_ptr<Instance> instance) {
  const VmaVulkanFunctions vmaVulkanFunctions = {
    vkGetInstanceProcAddr,
    vkGetDeviceProcAddr,
    vkGetPhysicalDeviceProperties,
    vkGetPhysicalDeviceMemoryProperties,
    vkAllocateMemory,
    vkFreeMemory,
    vkMapMemory,
    vkUnmapMemory,
    vkFlushMappedMemoryRanges,
    vkInvalidateMappedMemoryRanges,
    vkBindBufferMemory,
    vkBindImageMemory,
    vkGetBufferMemoryRequirements,
    vkGetImageMemoryRequirements,
    vkCreateBuffer,
    vkDestroyBuffer,
    vkCreateImage,
    vkDestroyImage,
    vkCmdCopyBuffer,
#if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
    /// Fetch "vkGetBufferMemoryRequirements2" on Vulkan >= 1.1, fetch "vkGetBufferMemoryRequirements2KHR" when using
    /// VK_KHR_dedicated_allocation extension.
    vkGetBufferMemoryRequirements2KHR,
    /// Fetch "vkGetImageMemoryRequirements 2" on Vulkan >= 1.1, fetch "vkGetImageMemoryRequirements2KHR" when using
    /// VK_KHR_dedicated_allocation extension.
    vkGetImageMemoryRequirements2KHR,
#endif
#if VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000
    vkBindBufferMemory2KHR,
    /// Fetch "vkBindImageMemory2" on Vulkan >= 1.1, fetch "vkBindImageMemory2KHR" when using VK_KHR_bind_memory2
    /// extension.
    vkBindImageMemory2KHR,
#endif
#if VMA_MEMORY_BUDGET || VMA_VULKAN_VERSION >= 1001000
    vkGetPhysicalDeviceMemoryProperties2KHR
#endif
  };
  VmaAllocatorCreateInfo createInfo{
      .physicalDevice = device->getDevice().physical_device,
      .device = device->getDevice().device,
      .pVulkanFunctions = &vmaVulkanFunctions,
      .instance = instance->getInstance().instance,
  };

  if (vmaCreateAllocator(&createInfo, &_allocator) != VK_SUCCESS) {
    throw std::runtime_error("Can't create vma allocator");
  }
}

const VmaAllocator& MemoryAllocator::getAllocator() { return _allocator; }

MemoryAllocator::~MemoryAllocator() { vmaDestroyAllocator(_allocator); }