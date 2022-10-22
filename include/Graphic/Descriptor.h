#pragma once
#include "Device.h"
#include "Buffer.h"
#include "Texture.h"

class DescriptorSetLayout {
 private:
  std::shared_ptr<Device> _device;
  VkDescriptorSetLayout _descriptorSetLayout;

 public:
  DescriptorSetLayout(std::shared_ptr<Device> device);
  VkDescriptorSetLayout& getDescriptorSetLayout();
  ~DescriptorSetLayout();
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

class DescriptorSet {
 private:
  std::vector<VkDescriptorSet> _descriptorSets;

 public:
  DescriptorSet(int number,
                std::shared_ptr<Texture> texture,
                std::shared_ptr<UniformBuffer> uniformBuffer,
                std::shared_ptr<DescriptorSetLayout> layout,
                std::shared_ptr<DescriptorPool> pool,
                std::shared_ptr<Device> device);
  std::vector<VkDescriptorSet> getDescriptorSets();
};