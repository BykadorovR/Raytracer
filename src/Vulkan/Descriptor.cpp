#include "Vulkan/Descriptor.h"

DescriptorSetLayout::DescriptorSetLayout(std::shared_ptr<Device> device) { _device = device; }

void DescriptorSetLayout::createCustom(std::vector<VkDescriptorSetLayoutBinding> info) {
  _info = info;

  auto layoutInfo = VkDescriptorSetLayoutCreateInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                    .bindingCount = static_cast<uint32_t>(_info.size()),
                                                    .pBindings = _info.data()};
  if (vkCreateDescriptorSetLayout(_device->getLogicalDevice(), &layoutInfo, nullptr, &_descriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor set layout!");
  }
}

std::vector<VkDescriptorSetLayoutBinding> DescriptorSetLayout::getLayoutInfo() { return _info; }

VkDescriptorSetLayout& DescriptorSetLayout::getDescriptorSetLayout() { return _descriptorSetLayout; }

DescriptorSetLayout::~DescriptorSetLayout() {
  vkDestroyDescriptorSetLayout(_device->getLogicalDevice(), _descriptorSetLayout, nullptr);
}

DescriptorSet::DescriptorSet(std::shared_ptr<DescriptorSetLayout> layout, std::shared_ptr<EngineState> engineState) {
  _engineState = engineState;
  _layoutInfo = layout->getLayoutInfo();

  _engineState->getDescriptorPool()->notify(_layoutInfo, 1);

  VkDescriptorSetAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                        .descriptorPool = _engineState->getDescriptorPool()->getDescriptorPool(),
                                        .descriptorSetCount = 1,
                                        .pSetLayouts = &layout->getDescriptorSetLayout()};
  auto sts = vkAllocateDescriptorSets(_engineState->getDevice()->getLogicalDevice(), &allocInfo, &_descriptorSet);
  if (sts != VK_SUCCESS) {
    std::string descriptors = "";
    for (auto [key, value] : _engineState->getDescriptorPool()->getDescriptorsNumber()) {
      descriptors += std::to_string(key) + ":" + std::to_string(value) + " ";
    }
    throw std::runtime_error("failed to allocate descriptor sets: allocated sets: " +
                             std::to_string(_engineState->getDescriptorPool()->getDescriptorSetsNumber()) +
                             ", descriptors: " + descriptors);
  }
}

void DescriptorSet::createCustom(std::map<int, std::vector<VkDescriptorBufferInfo>> buffers,
                                 std::map<int, std::vector<VkDescriptorImageInfo>> images) {
  std::vector<VkWriteDescriptorSet> descriptorWrites;
  for (auto [key, value] : buffers) {
    VkWriteDescriptorSet descriptorSet = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                          .dstSet = _descriptorSet,
                                          .dstBinding = _layoutInfo[key].binding,
                                          .dstArrayElement = 0,
                                          .descriptorCount = _layoutInfo[key].descriptorCount,
                                          .descriptorType = _layoutInfo[key].descriptorType,
                                          .pBufferInfo = buffers[key].data()};
    descriptorWrites.push_back(descriptorSet);
  }
  for (auto [key, value] : images) {
    VkWriteDescriptorSet descriptorSet = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                          .dstSet = _descriptorSet,
                                          .dstBinding = _layoutInfo[key].binding,
                                          .dstArrayElement = 0,
                                          .descriptorCount = _layoutInfo[key].descriptorCount,
                                          .descriptorType = _layoutInfo[key].descriptorType,
                                          .pImageInfo = images[key].data()};
    descriptorWrites.push_back(descriptorSet);
  }

  vkUpdateDescriptorSets(_engineState->getDevice()->getLogicalDevice(), descriptorWrites.size(),
                         descriptorWrites.data(), 0, nullptr);
}

VkDescriptorSet& DescriptorSet::getDescriptorSets() { return _descriptorSet; }

DescriptorSet::~DescriptorSet() {
  _engineState->getDescriptorPool()->notify(_layoutInfo, -1);
  vkFreeDescriptorSets(_engineState->getDevice()->getLogicalDevice(),
                       _engineState->getDescriptorPool()->getDescriptorPool(), 1, &_descriptorSet);
}