#include "Sync.h"

Semaphore::Semaphore(VkSemaphoreType type, std::shared_ptr<Device> device) {
  _device = device;
  _type = type;
  VkSemaphoreTypeCreateInfo timelineCreateInfo{};
  timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
  timelineCreateInfo.pNext = NULL;
  timelineCreateInfo.semaphoreType = type;
  timelineCreateInfo.initialValue = 0;

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphoreInfo.pNext = &timelineCreateInfo;
  semaphoreInfo.flags = 0;

  if (vkCreateSemaphore(device->getLogicalDevice(), &semaphoreInfo, nullptr, &_semaphore) != VK_SUCCESS)
    throw std::runtime_error("failed to create semaphore!");
}

void Semaphore::reset() {
  vkDestroySemaphore(_device->getLogicalDevice(), _semaphore, nullptr);
  VkSemaphoreTypeCreateInfo timelineCreateInfo{};
  timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
  timelineCreateInfo.pNext = NULL;
  timelineCreateInfo.semaphoreType = _type;
  timelineCreateInfo.initialValue = 0;

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphoreInfo.pNext = &timelineCreateInfo;
  semaphoreInfo.flags = 0;

  if (vkCreateSemaphore(_device->getLogicalDevice(), &semaphoreInfo, nullptr, &_semaphore) != VK_SUCCESS)
    throw std::runtime_error("failed to create semaphore!");
}

VkSemaphore& Semaphore::getSemaphore() { return _semaphore; }

Semaphore::~Semaphore() { vkDestroySemaphore(_device->getLogicalDevice(), _semaphore, nullptr); }

Fence::Fence(std::shared_ptr<Device> device) {
  _device = device;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  if (vkCreateFence(device->getLogicalDevice(), &fenceInfo, nullptr, &_fence) != VK_SUCCESS)
    throw std::runtime_error("failed to create synchronization objects for a frame!");
}

VkFence& Fence::getFence() { return _fence; }

Fence::~Fence() { vkDestroyFence(_device->getLogicalDevice(), _fence, nullptr); }