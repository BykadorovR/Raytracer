#include "Queue.h"

Queue::Queue(std::shared_ptr<Device> device) {
  vkGetDeviceQueue(device->getLogicalDevice(), device->getSupportedGraphicsFamilyIndex().value(), 0, &_graphicQueue);
  vkGetDeviceQueue(device->getLogicalDevice(), device->getSupportedPresentFamilyIndex().value(), 0, &_presentQueue);
}

VkQueue& Queue::getGraphicQueue() { return _graphicQueue; }

VkQueue& Queue::getPresentQueue() { return _presentQueue; }
