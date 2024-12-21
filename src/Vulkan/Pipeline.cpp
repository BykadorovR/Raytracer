#include "Vulkan/Pipeline.h"
#include "Vulkan/Buffer.h"
#include <ranges>

Pipeline::Pipeline(std::shared_ptr<Device> device) { _device = device; }

std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>>& Pipeline::getDescriptorSetLayout() {
  return _descriptorSetLayout;
}

std::map<std::string, VkPushConstantRange>& Pipeline::getPushConstants() { return _pushConstants; }

VkPipeline& Pipeline::getPipeline() { return _pipeline; }

VkPipelineLayout& Pipeline::getPipelineLayout() { return _pipelineLayout; }

Pipeline::~Pipeline() {
  vkDestroyPipeline(_device->getLogicalDevice(), _pipeline, nullptr);
  vkDestroyPipelineLayout(_device->getLogicalDevice(), _pipelineLayout, nullptr);
}

PipelineGraphic::PipelineGraphic(std::shared_ptr<Device> device) : Pipeline(device) {
  _inputAssembly = VkPipelineInputAssemblyStateCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE};
  _viewportState = VkPipelineViewportStateCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                     .viewportCount = 1,
                                                     .scissorCount = 1};
  _rasterizer = VkPipelineRasterizationStateCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .lineWidth = 1.0f};
  _multisampling = VkPipelineMultisampleStateCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE};
  _blendAttachmentState = VkPipelineColorBlendAttachmentState{
      .blendEnable = VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                        VK_COLOR_COMPONENT_A_BIT};
  _colorBlending = VkPipelineColorBlendStateCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &_blendAttachmentState,
      .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}};
  _depthStencil = VkPipelineDepthStencilStateCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_FALSE,
      .depthWriteEnable = VK_FALSE,
      .depthCompareOp = VK_COMPARE_OP_LESS,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
      .front = {},             // Optional
      .back = {},              // Optional
      .minDepthBounds = 0.0f,  // Optional
      .maxDepthBounds = 1.0f,  // Optional
  };
  _dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS};
  _dynamicState = VkPipelineDynamicStateCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                   .dynamicStateCount = static_cast<uint32_t>(_dynamicStates.size()),
                                                   .pDynamicStates = _dynamicStates.data()};
}

void PipelineGraphic::setCullMode(VkCullModeFlags cullMode) { _rasterizer.cullMode = cullMode; }

void PipelineGraphic::setPolygonMode(VkPolygonMode polygonMode) { _rasterizer.polygonMode = polygonMode; }

void PipelineGraphic::setAlphaBlending(bool alphaBlending) { _blendAttachmentState.blendEnable = alphaBlending; }

void PipelineGraphic::setTopology(VkPrimitiveTopology topology) { _inputAssembly.topology = topology; }

void PipelineGraphic::setDepthBias(bool depthBias) { _rasterizer.depthBiasEnable = depthBias; }

void PipelineGraphic::setDepthTest(bool depthTest) { _depthStencil.depthTestEnable = depthTest; }

void PipelineGraphic::setDepthWrite(bool depthWrite) { _depthStencil.depthWriteEnable = depthWrite; }

void PipelineGraphic::setDepthCompateOp(VkCompareOp depthCompareOp) {
  // we force skybox to have the biggest possible depth = 1 so we need to draw skybox if it's depth <= 1
  _depthStencil.depthCompareOp = depthCompareOp;
}

void PipelineGraphic::setColorBlendOp(VkBlendOp colorBlendOp) { _blendAttachmentState.colorBlendOp = colorBlendOp; }

void PipelineGraphic::setTesselation(int patchControlPoints) {
  _tessellationState = VkPipelineTessellationStateCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .patchControlPoints = static_cast<uint32_t>(patchControlPoints)};
}

void PipelineGraphic::createCustom(
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
    std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
    std::map<std::string, VkPushConstantRange> pushConstants,
    VkVertexInputBindingDescription bindingDescription,
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions,
    std::shared_ptr<RenderPass> renderPass) {
  _descriptorSetLayout = descriptorSetLayout;
  _pushConstants = pushConstants;

  // create pipeline layout
  std::vector<VkDescriptorSetLayout> descriptorSetLayoutRaw;
  for (auto& layout : _descriptorSetLayout) {
    descriptorSetLayoutRaw.push_back(layout.second->getDescriptorSetLayout());
  }
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                .setLayoutCount = static_cast<uint32_t>(descriptorSetLayoutRaw.size()),
                                                .pSetLayouts = descriptorSetLayoutRaw.data()};

  auto pushConstantsView = std::views::values(pushConstants);
  auto pushConstantsRaw = std::vector<VkPushConstantRange>{pushConstantsView.begin(), pushConstantsView.end()};
  if (pushConstants.size() > 0) {
    pipelineLayoutInfo.pPushConstantRanges = pushConstantsRaw.data();
    pipelineLayoutInfo.pushConstantRangeCount = pushConstantsRaw.size();
  }
  if (vkCreatePipelineLayout(_device->getLogicalDevice(), &pipelineLayoutInfo, nullptr, &_pipelineLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }

  // create pipeline
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &bindingDescription,
      .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
      .pVertexAttributeDescriptions = attributeDescriptions.data()};

  std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(renderPass->getColorAttachmentNumber(),
                                                                    _blendAttachmentState);
  _colorBlending.attachmentCount = blendAttachments.size();
  _colorBlending.pAttachments = blendAttachments.data();

  VkGraphicsPipelineCreateInfo pipelineInfo{.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                            .stageCount = static_cast<uint32_t>(shaderStages.size()),
                                            .pStages = shaderStages.data(),
                                            .pVertexInputState = &vertexInputInfo,
                                            .pInputAssemblyState = &_inputAssembly,
                                            .pViewportState = &_viewportState,
                                            .pRasterizationState = &_rasterizer,
                                            .pMultisampleState = &_multisampling,
                                            .pDepthStencilState = &_depthStencil,
                                            .pColorBlendState = &_colorBlending,
                                            .pDynamicState = &_dynamicState,
                                            .layout = _pipelineLayout,
                                            .renderPass = renderPass->getRenderPass(),
                                            .subpass = 0,
                                            .basePipelineHandle = VK_NULL_HANDLE};
  if (_tessellationState) pipelineInfo.pTessellationState = &_tessellationState.value();
  auto status = vkCreateGraphicsPipelines(_device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                          &_pipeline);
  if (status != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }
}

PipelineCompute::PipelineCompute(std::shared_ptr<Device> device) : Pipeline(device) {}

void PipelineCompute::createCustom(
    VkPipelineShaderStageCreateInfo shaderStage,
    std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
    std::map<std::string, VkPushConstantRange> pushConstants) {
  _descriptorSetLayout = descriptorSetLayout;
  _pushConstants = pushConstants;

  // create pipeline layout
  std::vector<VkDescriptorSetLayout> descriptorSetLayoutRaw;
  for (auto& layout : _descriptorSetLayout) {
    descriptorSetLayoutRaw.push_back(layout.second->getDescriptorSetLayout());
  }
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = descriptorSetLayout.size();
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayoutRaw.data();

  auto pushConstantsView = std::views::values(pushConstants);
  auto pushConstantsRaw = std::vector<VkPushConstantRange>{pushConstantsView.begin(), pushConstantsView.end()};
  if (pushConstants.size() > 0) {
    pipelineLayoutInfo.pPushConstantRanges = pushConstantsRaw.data();
    pipelineLayoutInfo.pushConstantRangeCount = pushConstantsRaw.size();
  }

  if (vkCreatePipelineLayout(_device->getLogicalDevice(), &pipelineLayoutInfo, nullptr, &_pipelineLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline layout!");
  }

  VkComputePipelineCreateInfo computePipelineCreateInfo{};
  computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.layout = _pipelineLayout;
  computePipelineCreateInfo.flags = 0;
  //
  computePipelineCreateInfo.stage = shaderStage;
  vkCreateComputePipelines(_device->getLogicalDevice(), VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr,
                           &_pipeline);
}