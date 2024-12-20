#pragma once
#include "Vulkan/Device.h"
#include "Utility/Settings.h"

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
  std::map<VkDescriptorType, int> _descriptorTypes;
  int _descriptorSetsNumber = 0;

 public:
  DescriptorPool(std::shared_ptr<Settings> settings, std::shared_ptr<Device> device);
  void notify(std::vector<VkDescriptorSetLayoutBinding> layoutInfo, int number);
  // needed to calculate real number of descriptor sets and descriptors
  std::map<VkDescriptorType, int> getDescriptorsNumber();
  int getDescriptorSetsNumber();
  VkDescriptorPool& getDescriptorPool();
  ~DescriptorPool();
};