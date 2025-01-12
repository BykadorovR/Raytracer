#pragma once
#include "Vulkan/Device.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/Pool.h"
#include "Graphic/Texture.h"

class DescriptorSetLayout {
 private:
  std::shared_ptr<Device> _device;
  VkDescriptorSetLayout _descriptorSetLayout;
  std::vector<VkDescriptorSetLayoutBinding> _info;

 public:
  DescriptorSetLayout(std::shared_ptr<Device> device);
  void createCustom(std::vector<VkDescriptorSetLayoutBinding> info);
  std::vector<VkDescriptorSetLayoutBinding> getLayoutInfo();
  VkDescriptorSetLayout& getDescriptorSetLayout();
  ~DescriptorSetLayout();
};

class DescriptorSet {
 private:
  VkDescriptorSet _descriptorSet;
  std::shared_ptr<EngineState> _engineState;
  std::vector<VkDescriptorSetLayoutBinding> _layoutInfo;

 public:
  DescriptorSet(std::shared_ptr<DescriptorSetLayout> layout, std::shared_ptr<EngineState> engineState);
  void createCustom(std::map<int, std::vector<VkDescriptorBufferInfo>> buffers,
                    std::map<int, std::vector<VkDescriptorImageInfo>> images);
  VkDescriptorSet& getDescriptorSets();
  ~DescriptorSet();
};