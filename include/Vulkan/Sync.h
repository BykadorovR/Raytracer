#pragma once
#include "Vulkan/Device.h"

class Semaphore {
 private:
  std::shared_ptr<Device> _device;
  VkSemaphore _semaphore;

 public:
  Semaphore(std::shared_ptr<Device> device);
  VkSemaphore& getSemaphore();
  ~Semaphore();
};

class Fence {
 private:
  std::shared_ptr<Device> _device;
  VkFence _fence;

 public:
  Fence(std::shared_ptr<Device> device);
  VkFence& getFence();
  ~Fence();
};