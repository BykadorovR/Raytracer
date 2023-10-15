#pragma once
#include "Settings.h"
#include "Shader.h"
#include "Buffer.h"
#include "Descriptor.h"

class Pipeline {
 private:
  std::shared_ptr<Settings> _settings;
  std::shared_ptr<Device> _device;
  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> _descriptorSetLayout;
  std::map<std::string, VkPushConstantRange> _pushConstants;
  std::vector<VkDynamicState> _dynamicStates;
  VkPipeline _pipeline;
  VkPipelineLayout _pipelineLayout;

  VkPipelineDynamicStateCreateInfo _dynamicState;
  VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
  VkPipelineViewportStateCreateInfo _viewportState;
  VkPipelineRasterizationStateCreateInfo _rasterizer;
  VkPipelineMultisampleStateCreateInfo _multisampling;
  VkPipelineColorBlendAttachmentState _blendAttachmentState;
  VkPipelineColorBlendStateCreateInfo _colorBlending;
  VkPipelineDepthStencilStateCreateInfo _depthStencil;

 public:
  Pipeline(std::shared_ptr<Settings> settings, std::shared_ptr<Device> device);
  void createGraphic2D(std::vector<VkFormat> renderFormat,
                       VkCullModeFlags cullMode,
                       VkPolygonMode polygonMode,
                       std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
                       std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
                       std::map<std::string, VkPushConstantRange> pushConstants,
                       VkVertexInputBindingDescription bindingDescription,
                       std::vector<VkVertexInputAttributeDescription> attributeDescriptions);
  void createGraphic2DShadow(
      VkCullModeFlags cullMode,
      std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
      std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
      std::map<std::string, VkPushConstantRange> pushConstants,
      VkVertexInputBindingDescription bindingDescription,
      std::vector<VkVertexInputAttributeDescription> attributeDescriptions);
  void createGraphic3D(std::vector<VkFormat> renderFormat,
                       VkCullModeFlags cullMode,
                       VkPolygonMode polygonMode,
                       std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
                       std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
                       std::map<std::string, VkPushConstantRange> pushConstants,
                       VkVertexInputBindingDescription bindingDescription,
                       std::vector<VkVertexInputAttributeDescription> attributeDescriptions);
  void createLine(std::vector<VkFormat> renderFormat,
                  VkCullModeFlags cullMode,
                  VkPolygonMode polygonMode,
                  int thick,
                  std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
                  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
                  std::map<std::string, VkPushConstantRange> pushConstants,
                  VkVertexInputBindingDescription bindingDescription,
                  std::vector<VkVertexInputAttributeDescription> attributeDescriptions);

  void createGraphicTerrainGPU(
      std::vector<VkFormat> renderFormat,
      VkCullModeFlags cullMode,
      VkPolygonMode polygonMode,
      std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
      std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
      std::map<std::string, VkPushConstantRange> pushConstants,
      VkVertexInputBindingDescription bindingDescription,
      std::vector<VkVertexInputAttributeDescription> attributeDescriptions);

  void createGraphicTerrainShadowGPU(
      VkCullModeFlags cullMode,
      VkPolygonMode polygonMode,
      std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
      std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
      std::map<std::string, VkPushConstantRange> pushConstants,
      VkVertexInputBindingDescription bindingDescription,
      std::vector<VkVertexInputAttributeDescription> attributeDescriptions);

  void createGraphic3DShadow(
      VkCullModeFlags cullMode,
      std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
      std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
      std::map<std::string, VkPushConstantRange> pushConstants,
      VkVertexInputBindingDescription bindingDescription,
      std::vector<VkVertexInputAttributeDescription> attributeDescriptions);
  void createHUD(VkFormat renderFormat,
                 std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
                 std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
                 std::map<std::string, VkPushConstantRange> pushConstants,
                 VkVertexInputBindingDescription bindingDescription,
                 std::vector<VkVertexInputAttributeDescription> attributeDescriptions);
  void createParticleSystemCompute(
      VkPipelineShaderStageCreateInfo shaderStage,
      std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
      std::map<std::string, VkPushConstantRange> pushConstants);
  void createParticleSystemGraphic(
      std::vector<VkFormat> renderFormat,
      VkCullModeFlags cullMode,
      VkPolygonMode polygonMode,
      std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
      std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
      std::map<std::string, VkPushConstantRange> pushConstants,
      VkVertexInputBindingDescription bindingDescription,
      std::vector<VkVertexInputAttributeDescription> attributeDescriptions);

  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>>& getDescriptorSetLayout();
  std::map<std::string, VkPushConstantRange>& getPushConstants();
  VkPipeline& getPipeline();
  VkPipelineLayout& getPipelineLayout();
  ~Pipeline();
};