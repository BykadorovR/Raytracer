#pragma once
#include "Device.h"
#include "Shader.h"
#include "Buffer.h"
#include "Render.h"
#include "Descriptor.h"

class Pipeline {
 private:
  std::shared_ptr<Device> _device;
  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> _descriptorSetLayout;
  std::map<std::string, VkPushConstantRange> _pushConstants;
  VkPipeline _pipeline;
  VkPipelineLayout _pipelineLayout;

  VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
  VkPipelineViewportStateCreateInfo _viewportState;
  VkPipelineRasterizationStateCreateInfo _rasterizer;
  VkPipelineMultisampleStateCreateInfo _multisampling;
  VkPipelineColorBlendAttachmentState _blendAttachmentState;
  VkPipelineColorBlendStateCreateInfo _colorBlending;
  VkPipelineDepthStencilStateCreateInfo _depthStencil;

 public:
  Pipeline(std::shared_ptr<Device> device);
  void createGraphic2D(VkCullModeFlags cullMode,
                       std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
                       std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
                       std::map<std::string, VkPushConstantRange> pushConstants,
                       VkVertexInputBindingDescription bindingDescription,
                       std::vector<VkVertexInputAttributeDescription> attributeDescriptions,
                       std::shared_ptr<RenderPass> renderPass);
  void createGraphic2DShadow(
      VkCullModeFlags cullMode,
      std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
      std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
      std::map<std::string, VkPushConstantRange> pushConstants,
      VkVertexInputBindingDescription bindingDescription,
      std::vector<VkVertexInputAttributeDescription> attributeDescriptions,
      std::shared_ptr<RenderPass> renderPass);
  void createGraphic3D(VkCullModeFlags cullMode,
                       std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
                       std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
                       std::map<std::string, VkPushConstantRange> pushConstants,
                       VkVertexInputBindingDescription bindingDescription,
                       std::array<VkVertexInputAttributeDescription, 7> attributeDescriptions,
                       std::shared_ptr<RenderPass> renderPass);
  void createGraphic3DShadow(
      VkCullModeFlags cullMode,
      std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
      std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
      std::map<std::string, VkPushConstantRange> pushConstants,
      VkVertexInputBindingDescription bindingDescription,
      std::array<VkVertexInputAttributeDescription, 7> attributeDescriptions,
      std::shared_ptr<RenderPass> renderPass);
  void createHUD(std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
                 std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
                 std::map<std::string, VkPushConstantRange> pushConstants,
                 VkVertexInputBindingDescription bindingDescription,
                 std::vector<VkVertexInputAttributeDescription> attributeDescriptions,
                 std::shared_ptr<RenderPass> renderPass);
  void createCompute(VkPipelineShaderStageCreateInfo shaderStage,
                     std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout);

  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>>& getDescriptorSetLayout();
  std::map<std::string, VkPushConstantRange>& getPushConstants();
  VkPipeline& getPipeline();
  VkPipelineLayout& getPipelineLayout();
  ~Pipeline();
};