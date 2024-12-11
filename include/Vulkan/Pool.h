#pragma once
#include "Vulkan/Device.h"

class CommandPool {
 private:
  std::shared_ptr<Device> _device;
  vkb::QueueType _type;
  VkCommandPool _commandPool;

 public:
  CommandPool(vkb::QueueType type, std::shared_ptr<Device> device);
  VkCommandPool& getCommandPool();
  vkb::QueueType getType();
  ~CommandPool();
};

class DescriptorPool {
 private:
  std::shared_ptr<Device> _device;
  VkDescriptorPool _descriptorPool;

 public:
  DescriptorPool(int number, std::shared_ptr<Device> device);
  VkDescriptorPool& getDescriptorPool();
  ~DescriptorPool();
};