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
  std::vector<VkDescriptorSet> _descriptorSets;
  std::shared_ptr<EngineState> _engineState;
  std::vector<VkDescriptorSetLayoutBinding> _layoutInfo;

 public:
  DescriptorSet(int number, std::shared_ptr<DescriptorSetLayout> layout, std::shared_ptr<EngineState> engineState);
  void createCustom(int currentFrame,
                    std::map<int, std::vector<VkDescriptorBufferInfo>> buffers,
                    std::map<int, std::vector<VkDescriptorImageInfo>> images);
  std::vector<VkDescriptorSet>& getDescriptorSets();
  ~DescriptorSet();
};