#pragma once
#include "Utility/Settings.h"
#include "Vulkan/Shader.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/Descriptor.h"
#include "Vulkan/Render.h"

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
  void createGraphic2D(VkCullModeFlags cullMode,
                       VkPolygonMode polygonMode,
                       bool enableAlphaBlending,
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
                       VkPolygonMode polygonMode,
                       std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
                       std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
                       std::map<std::string, VkPushConstantRange> pushConstants,
                       VkVertexInputBindingDescription bindingDescription,
                       std::vector<VkVertexInputAttributeDescription> attributeDescriptions,
                       std::shared_ptr<RenderPass> renderPass);
  void createGeometry(VkCullModeFlags cullMode,
                      VkPolygonMode polygonMode,
                      VkPrimitiveTopology topology,
                      std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
                      std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
                      std::map<std::string, VkPushConstantRange> pushConstants,
                      VkVertexInputBindingDescription bindingDescription,
                      std::vector<VkVertexInputAttributeDescription> attributeDescriptions,
                      std::shared_ptr<RenderPass> renderPass);
  void createSkybox(VkCullModeFlags cullMode,
                    VkPolygonMode polygonMode,
                    std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
                    std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
                    std::map<std::string, VkPushConstantRange> pushConstants,
                    VkVertexInputBindingDescription bindingDescription,
                    std::vector<VkVertexInputAttributeDescription> attributeDescriptions,
                    std::shared_ptr<RenderPass> renderPass);
  void createGraphicTerrain(
      VkCullModeFlags cullMode,
      VkPolygonMode polygonMode,
      std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
      std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
      std::map<std::string, VkPushConstantRange> pushConstants,
      VkVertexInputBindingDescription bindingDescription,
      std::vector<VkVertexInputAttributeDescription> attributeDescriptions,
      std::shared_ptr<RenderPass> renderPass);
  void createGraphicTerrainShadowGPU(
      VkCullModeFlags cullMode,
      VkPolygonMode polygonMode,
      std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
      std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
      std::map<std::string, VkPushConstantRange> pushConstants,
      VkVertexInputBindingDescription bindingDescription,
      std::vector<VkVertexInputAttributeDescription> attributeDescriptions,
      std::shared_ptr<RenderPass> renderPass);
  void createGraphic3DShadow(
      VkCullModeFlags cullMode,
      std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
      std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
      std::map<std::string, VkPushConstantRange> pushConstants,
      VkVertexInputBindingDescription bindingDescription,
      std::vector<VkVertexInputAttributeDescription> attributeDescriptions,
      std::shared_ptr<RenderPass> renderPass);
  void createHUD(std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
                 std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
                 std::map<std::string, VkPushConstantRange> pushConstants,
                 VkVertexInputBindingDescription bindingDescription,
                 std::vector<VkVertexInputAttributeDescription> attributeDescriptions,
                 std::shared_ptr<RenderPass> renderPass);
  void createParticleSystemCompute(
      VkPipelineShaderStageCreateInfo shaderStage,
      std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
      std::map<std::string, VkPushConstantRange> pushConstants);
  void createParticleSystemGraphic(
      VkCullModeFlags cullMode,
      VkPolygonMode polygonMode,
      std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
      std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
      std::map<std::string, VkPushConstantRange> pushConstants,
      VkVertexInputBindingDescription bindingDescription,
      std::vector<VkVertexInputAttributeDescription> attributeDescriptions,
      std::shared_ptr<RenderPass> renderPass);

  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>>& getDescriptorSetLayout();
  std::map<std::string, VkPushConstantRange>& getPushConstants();
  VkPipeline& getPipeline();
  VkPipelineLayout& getPipelineLayout();
  ~Pipeline();
};