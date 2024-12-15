#pragma once
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include "vk_mem_alloc.h"
#include "Vulkan/Device.h"
#include "Vulkan/Instance.h"
#undef min
#undef max

class MemoryAllocator {
 private:
  VmaAllocator _allocator;

 public:
  MemoryAllocator(std::shared_ptr<Device> device, std::shared_ptr<Instance> instance);
  const VmaAllocator& getAllocator();
  ~MemoryAllocator();
};