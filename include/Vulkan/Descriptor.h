#pragma once
#include "Device.h"
#include "Buffer.h"
#include "Texture.h"
#include "Pool.h"

class DescriptorSetLayout {
 private:
  std::shared_ptr<Device> _device;
  VkDescriptorSetLayout _descriptorSetLayout;
  std::vector<VkDescriptorSetLayoutBinding> _info;

 public:
  DescriptorSetLayout(std::shared_ptr<Device> device);

  void createCustom(std::vector<VkDescriptorSetLayoutBinding> info);
  void createTexture(int number = 1, int binding = 0, VkShaderStageFlags stage = VK_SHADER_STAGE_FRAGMENT_BIT);
  void createUniformBuffer(VkShaderStageFlags stage = VK_SHADER_STAGE_VERTEX_BIT);
  void createShaderStorageBuffer(std::vector<int> bindings, std::vector<VkShaderStageFlags> stage);
  void createParticleComputeBuffer();
  void createPostprocessing();
  void createGraphicModel();
  void createJoints();
  void createShadowTexture();
  void createModelAuxilary();
  void createGUI();
  std::vector<VkDescriptorSetLayoutBinding> getLayoutInfo();
  VkDescriptorSetLayout& getDescriptorSetLayout();
  ~DescriptorSetLayout();
};

class DescriptorSet {
 private:
  std::vector<VkDescriptorSet> _descriptorSets;
  std::shared_ptr<Device> _device;
  std::shared_ptr<DescriptorSetLayout> _layout;

 public:
  DescriptorSet(int number,
                std::shared_ptr<DescriptorSetLayout> layout,
                std::shared_ptr<DescriptorPool> pool,
                std::shared_ptr<Device> device);
  void createCustom(int currentFrame,
                    std::map<int, std::vector<VkDescriptorBufferInfo>> buffers,
                    std::map<int, std::vector<VkDescriptorImageInfo>> images);
  void createUniformBuffer(std::shared_ptr<UniformBuffer> uniformBuffer);
  void createParticleComputeBuffer(std::shared_ptr<UniformBuffer> uniformBuffer,
                                   std::shared_ptr<Buffer> bufferIn,
                                   std::shared_ptr<Buffer> bufferOut);
  void createTexture(std::vector<std::shared_ptr<Texture>> texture, int binding = 0);
  void createShaderStorageBuffer(int currentFrame,
                                 std::vector<int> bindings,
                                 std::vector<std::vector<std::shared_ptr<Buffer>>> buffer);
  void createPostprocessing(std::shared_ptr<ImageView> src,
                            std::shared_ptr<ImageView> blur,
                            std::shared_ptr<ImageView> dst);
  void createJoints(std::vector<std::shared_ptr<Buffer>> buffer);
  void createModelAuxilary(std::shared_ptr<UniformBuffer> uniformBuffer);
  void createGraphicModel(std::shared_ptr<Texture> texture, std::shared_ptr<Texture> normal);
  void createShadowTexture(std::vector<std::shared_ptr<Texture>> directional,
                           std::vector<std::shared_ptr<Texture>> point);
  void createBlur(std::vector<std::shared_ptr<Texture>> src, std::vector<std::shared_ptr<Texture>> dst);
  void createGUI(std::shared_ptr<Texture> texture, std::shared_ptr<UniformBuffer> uniformBuffer);
  std::vector<VkDescriptorSet>& getDescriptorSets();
};