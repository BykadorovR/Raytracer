#pragma once
#include "Device.h"

class Queue {
 private:
  VkQueue _graphicQueue, _presentQueue;

 public:
  Queue(std::shared_ptr<Device> device);
  VkQueue& getGraphicQueue();
  VkQueue& getPresentQueue();
};