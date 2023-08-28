#pragma once

#include "Device.h"

class Semaphore {
 private:
  std::shared_ptr<Device> _device;
  VkSemaphore _semaphore;
  VkSemaphoreType _type;

 public:
  Semaphore(VkSemaphoreType type, std::shared_ptr<Device> device);
  void reset();
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