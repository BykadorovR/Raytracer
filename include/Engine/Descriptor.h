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
  void createTexture(int number = 1, int binding = 0, VkShaderStageFlags stage = VK_SHADER_STAGE_FRAGMENT_BIT);
  void createBuffer(VkShaderStageFlags stage = VK_SHADER_STAGE_VERTEX_BIT);

  void createGraphicModel();
  void createJoints();
  void createLight();
  void createLightVP(VkShaderStageFlagBits stage);
  void createShadowTexture();
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
  void createBuffer(std::shared_ptr<UniformBuffer> uniformBuffer);
  void createTexture(std::vector<std::shared_ptr<Texture>> texture, int binding = 0);

  void createJoints(std::shared_ptr<Buffer> buffer);
  void createLight(std::shared_ptr<Buffer> bufferDirectional, std::shared_ptr<Buffer> bufferPoint);
  void createModelAuxilary(std::shared_ptr<UniformBuffer> uniformBuffer);
  void createGraphicModel(std::shared_ptr<Texture> texture, std::shared_ptr<Texture> normal);
  void createShadowTexture(std::vector<std::shared_ptr<Texture>> directional,
                           std::vector<std::shared_ptr<Texture>> point);

  void createCompute(std::vector<std::shared_ptr<Texture>> textureOut,
                     std::shared_ptr<UniformBuffer> uniformBuffer,
                     std::shared_ptr<UniformBuffer> uniformSpheres,
                     std::shared_ptr<UniformBuffer> uniformRectangles,
                     std::shared_ptr<UniformBuffer> uniformHitboxes,
                     std::shared_ptr<UniformBuffer> uniformSettings);
  void createGUI(std::shared_ptr<Texture> texture, std::shared_ptr<UniformBuffer> uniformBuffer);
  std::vector<VkDescriptorSet>& getDescriptorSets();
};