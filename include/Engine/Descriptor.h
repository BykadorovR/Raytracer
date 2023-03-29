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
  void createGraphic();
  void createCamera();
  void createJoints();
  void createModelAuxilary();
  void createCompute();
  void createGUI();
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
  std::shared_ptr<Device> _device;

 public:
  DescriptorSet(int number,
                std::shared_ptr<DescriptorSetLayout> layout,
                std::shared_ptr<DescriptorPool> pool,
                std::shared_ptr<Device> device);
  void createJoints(std::shared_ptr<Buffer> buffer);
  void createCamera(std::shared_ptr<UniformBuffer> uniformBuffer);
  void createModelAuxilary(std::shared_ptr<UniformBuffer> uniformBuffer);
  void createGraphic(std::shared_ptr<Texture> texture);
  void createCompute(std::vector<std::shared_ptr<Texture>> textureOut,
                     std::shared_ptr<UniformBuffer> uniformBuffer,
                     std::shared_ptr<UniformBuffer> uniformSpheres,
                     std::shared_ptr<UniformBuffer> uniformRectangles,
                     std::shared_ptr<UniformBuffer> uniformHitboxes,
                     std::shared_ptr<UniformBuffer> uniformSettings);
  void createGUI(std::shared_ptr<Texture> texture, std::shared_ptr<UniformBuffer> uniformBuffer);
  std::vector<VkDescriptorSet>& getDescriptorSets();
};