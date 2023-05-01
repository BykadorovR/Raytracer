#pragma once
#include "Device.h"
#include "Shader.h"
#include "Buffer.h"
#include "Render.h"
#include "Descriptor.h"

class Pipeline {
 private:
  std::shared_ptr<Device> _device;
  std::vector<std::shared_ptr<DescriptorSetLayout>> _descriptorSetLayout;
  std::map<std::string, VkPushConstantRange> _pushConstants;
  VkPipeline _pipeline;
  VkPipelineLayout _pipelineLayout;
  std::shared_ptr<Shader> _shader;

 public:
  Pipeline(std::shared_ptr<Shader> shader, std::shared_ptr<Device> device);
  void createGraphic2D(std::vector<std::shared_ptr<DescriptorSetLayout>> descriptorSetLayout,
                       std::map<std::string, VkPushConstantRange> pushConstants,
                       VkVertexInputBindingDescription bindingDescription,
                       std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions,
                       std::shared_ptr<RenderPass> renderPass);
  void createGraphic3D(VkCullModeFlags cullMode,
                       std::vector<std::shared_ptr<DescriptorSetLayout>> descriptorSetLayout,
                       std::map<std::string, VkPushConstantRange> pushConstants,
                       VkVertexInputBindingDescription bindingDescription,
                       std::array<VkVertexInputAttributeDescription, 7> attributeDescriptions,
                       std::shared_ptr<RenderPass> renderPass);
  void createHUD(std::vector<std::shared_ptr<DescriptorSetLayout>> descriptorSetLayout,
                 VkVertexInputBindingDescription bindingDescription,
                 std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions,
                 std::shared_ptr<RenderPass> renderPass);
  void createCompute(std::vector<std::shared_ptr<DescriptorSetLayout>> descriptorSetLayout);

  std::vector<std::shared_ptr<DescriptorSetLayout>>& getDescriptorSetLayout();
  std::map<std::string, VkPushConstantRange>& getPushConstants();
  VkPipeline& getPipeline();
  VkPipelineLayout& getPipelineLayout();
  ~Pipeline();
};