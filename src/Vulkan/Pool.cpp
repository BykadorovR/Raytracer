#include "Vulkan/Descriptor.h"

CommandPool::CommandPool(vkb::QueueType type, std::shared_ptr<Device> device) {
  _device = device;
  _type = type;
  VkCommandPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                   .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                   .queueFamilyIndex = static_cast<uint32_t>(device->getQueueIndex(type))};

  if (vkCreateCommandPool(device->getLogicalDevice(), &poolInfo, nullptr, &_commandPool) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics command pool!");
  }
}

vkb::QueueType CommandPool::getType() { return _type; }

VkCommandPool& CommandPool::getCommandPool() { return _commandPool; }

CommandPool::~CommandPool() { vkDestroyCommandPool(_device->getLogicalDevice(), _commandPool, nullptr); }

DescriptorPool::DescriptorPool(std::shared_ptr<Settings> settings, std::shared_ptr<Device> device) {
  _device = device;

  std::vector<VkDescriptorPoolSize> poolSizes{
      {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = static_cast<uint32_t>(settings->getPoolSizeUBO())},
      {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       .descriptorCount = static_cast<uint32_t>(settings->getPoolSizeSampler())},
      {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
       .descriptorCount = static_cast<uint32_t>(settings->getPoolSizeComputeImage())},
      {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
       .descriptorCount = static_cast<uint32_t>(settings->getPoolSizeSSBO())}};

  VkDescriptorPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                      .maxSets = static_cast<uint32_t>(settings->getPoolSizeDescriptorSets()),
                                      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                                      .pPoolSizes = poolSizes.data()};

  poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  if (vkCreateDescriptorPool(_device->getLogicalDevice(), &poolInfo, nullptr, &_descriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }
}

void DescriptorPool::notify(std::vector<VkDescriptorSetLayoutBinding> layoutInfo, int number) {
  for (const auto& info : layoutInfo) {
    _descriptorTypes[info.descriptorType] += number;
  }

  _descriptorSetsNumber += number;
}
std::map<VkDescriptorType, int> DescriptorPool::getDescriptorsNumber() { return _descriptorTypes; }

int DescriptorPool::getDescriptorSetsNumber() { return _descriptorSetsNumber; }

VkDescriptorPool& DescriptorPool::getDescriptorPool() { return _descriptorPool; }

DescriptorPool::~DescriptorPool() { vkDestroyDescriptorPool(_device->getLogicalDevice(), _descriptorPool, nullptr); }