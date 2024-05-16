#include "Pipeline.h"
#include "Buffer.h"
#include <ranges>

Pipeline::Pipeline(std::shared_ptr<Settings> settings, std::shared_ptr<Device> device) {
  _settings = settings;
  _device = device;

  _inputAssembly = VkPipelineInputAssemblyStateCreateInfo{};
  _inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  _inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  _inputAssembly.primitiveRestartEnable = VK_FALSE;

  _viewportState = VkPipelineViewportStateCreateInfo{};
  _viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  _viewportState.viewportCount = 1;
  _viewportState.scissorCount = 1;

  _rasterizer = VkPipelineRasterizationStateCreateInfo{};
  _rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  _rasterizer.depthClampEnable = VK_FALSE;
  _rasterizer.rasterizerDiscardEnable = VK_FALSE;
  _rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  _rasterizer.lineWidth = 1.0f;
  _rasterizer.cullMode = VK_CULL_MODE_NONE;
  _rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  _rasterizer.depthBiasEnable = VK_FALSE;

  _multisampling = VkPipelineMultisampleStateCreateInfo{};
  _multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  _multisampling.sampleShadingEnable = VK_FALSE;
  _multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  _blendAttachmentState = VkPipelineColorBlendAttachmentState{};
  _blendAttachmentState.blendEnable = VK_TRUE;
  _blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  _blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  _blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  _blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
  _blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  _blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  _blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

  _colorBlending = VkPipelineColorBlendStateCreateInfo{};
  _colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  _colorBlending.logicOpEnable = VK_FALSE;
  _colorBlending.logicOp = VK_LOGIC_OP_COPY;
  _colorBlending.attachmentCount = 1;
  _colorBlending.pAttachments = &_blendAttachmentState;
  _colorBlending.blendConstants[0] = 0.0f;
  _colorBlending.blendConstants[1] = 0.0f;
  _colorBlending.blendConstants[2] = 0.0f;
  _colorBlending.blendConstants[3] = 0.0f;

  _depthStencil = VkPipelineDepthStencilStateCreateInfo{};
  _depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  _depthStencil.depthTestEnable = VK_FALSE;
  _depthStencil.depthWriteEnable = VK_FALSE;
  _depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  _depthStencil.depthBoundsTestEnable = VK_FALSE;
  _depthStencil.minDepthBounds = 0.0f;  // Optional
  _depthStencil.maxDepthBounds = 1.0f;  // Optional
  _depthStencil.stencilTestEnable = VK_FALSE;
  _depthStencil.front = {};  // Optional
  _depthStencil.back = {};   // Optional

  _dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS};
  _dynamicState = VkPipelineDynamicStateCreateInfo{};
  _dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  _dynamicState.dynamicStateCount = static_cast<uint32_t>(_dynamicStates.size());
  _dynamicState.pDynamicStates = _dynamicStates.data();
}

void Pipeline::createHUD(std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
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
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = descriptorSetLayoutRaw.size();
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

  // create pipeline
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = shaderStages.size();
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &_inputAssembly;
  pipelineInfo.pViewportState = &_viewportState;
  pipelineInfo.pDepthStencilState = &_depthStencil;
  pipelineInfo.pRasterizationState = &_rasterizer;
  pipelineInfo.pMultisampleState = &_multisampling;
  pipelineInfo.pColorBlendState = &_colorBlending;
  pipelineInfo.pDynamicState = &_dynamicState;
  pipelineInfo.layout = _pipelineLayout;
  pipelineInfo.subpass = 0;
  pipelineInfo.renderPass = renderPass->getRenderPass();
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(_device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }
}

void Pipeline::createSkybox(
    VkCullModeFlags cullMode,
    VkPolygonMode polygonMode,
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
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = descriptorSetLayoutRaw.size();
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

  // create pipeline
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  _rasterizer.cullMode = cullMode;
  _rasterizer.polygonMode = polygonMode;

  _depthStencil.depthTestEnable = VK_TRUE;
  _depthStencil.depthWriteEnable = VK_FALSE;
  // we force skybox to have the biggest possible depth = 1 so we need to draw skybox if it's depth <= 1
  _depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

  std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(renderPass->getColorAttachmentNumber(),
                                                                    _blendAttachmentState);
  _colorBlending.attachmentCount = blendAttachments.size();
  _colorBlending.pAttachments = blendAttachments.data();
  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = shaderStages.size();
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &_inputAssembly;
  pipelineInfo.pViewportState = &_viewportState;
  pipelineInfo.pDepthStencilState = &_depthStencil;
  pipelineInfo.pRasterizationState = &_rasterizer;
  pipelineInfo.pMultisampleState = &_multisampling;
  pipelineInfo.pColorBlendState = &_colorBlending;
  pipelineInfo.pDynamicState = &_dynamicState;
  pipelineInfo.layout = _pipelineLayout;
  pipelineInfo.subpass = 0;
  pipelineInfo.renderPass = renderPass->getRenderPass();
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(_device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }
}

void Pipeline::createGraphic3D(
    VkCullModeFlags cullMode,
    VkPolygonMode polygonMode,
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
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = descriptorSetLayoutRaw.size();
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

  // create pipeline
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  _rasterizer.cullMode = cullMode;
  _rasterizer.polygonMode = polygonMode;

  _depthStencil.depthTestEnable = VK_TRUE;
  _depthStencil.depthWriteEnable = VK_TRUE;
  std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(renderPass->getColorAttachmentNumber(),
                                                                    _blendAttachmentState);
  _colorBlending.attachmentCount = blendAttachments.size();
  _colorBlending.pAttachments = blendAttachments.data();

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = shaderStages.size();
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &_inputAssembly;
  pipelineInfo.pViewportState = &_viewportState;
  pipelineInfo.pDepthStencilState = &_depthStencil;
  pipelineInfo.pRasterizationState = &_rasterizer;
  pipelineInfo.pMultisampleState = &_multisampling;
  pipelineInfo.pColorBlendState = &_colorBlending;
  pipelineInfo.pDynamicState = &_dynamicState;
  pipelineInfo.layout = _pipelineLayout;
  pipelineInfo.subpass = 0;
  pipelineInfo.renderPass = renderPass->getRenderPass();
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(_device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }
}

void Pipeline::createLine(VkCullModeFlags cullMode,
                          VkPolygonMode polygonMode,
                          int thick,
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
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = descriptorSetLayoutRaw.size();
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

  // create pipeline
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  _inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

  _rasterizer.cullMode = cullMode;
  _rasterizer.polygonMode = polygonMode;
  _rasterizer.lineWidth = thick;

  _depthStencil.depthTestEnable = VK_TRUE;
  _depthStencil.depthWriteEnable = VK_TRUE;
  std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(renderPass->getColorAttachmentNumber(),
                                                                    _blendAttachmentState);
  _colorBlending.attachmentCount = blendAttachments.size();
  _colorBlending.pAttachments = blendAttachments.data();

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = shaderStages.size();
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &_inputAssembly;
  pipelineInfo.pViewportState = &_viewportState;
  pipelineInfo.pDepthStencilState = &_depthStencil;
  pipelineInfo.pRasterizationState = &_rasterizer;
  pipelineInfo.pMultisampleState = &_multisampling;
  pipelineInfo.pColorBlendState = &_colorBlending;
  pipelineInfo.pDynamicState = &_dynamicState;
  pipelineInfo.layout = _pipelineLayout;
  pipelineInfo.subpass = 0;
  pipelineInfo.renderPass = renderPass->getRenderPass();
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(_device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }
}

void Pipeline::createGraphicTerrainShadowGPU(
    VkCullModeFlags cullMode,
    VkPolygonMode polygonMode,
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
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = descriptorSetLayoutRaw.size();
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayoutRaw.data();

  _inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;

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
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  _rasterizer.cullMode = cullMode;
  _rasterizer.polygonMode = polygonMode;

  _rasterizer.depthBiasEnable = VK_TRUE;
  _depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  _depthStencil.depthTestEnable = VK_TRUE;
  _depthStencil.depthWriteEnable = VK_TRUE;
  _colorBlending.attachmentCount = 0;

  VkPipelineTessellationStateCreateInfo tessellationState{};
  tessellationState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
  tessellationState.pNext = nullptr;
  tessellationState.flags = 0;
  tessellationState.patchControlPoints = 4;

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = shaderStages.size();
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &_inputAssembly;
  pipelineInfo.pViewportState = &_viewportState;
  pipelineInfo.pDepthStencilState = &_depthStencil;
  pipelineInfo.pRasterizationState = &_rasterizer;
  pipelineInfo.pMultisampleState = &_multisampling;
  pipelineInfo.pColorBlendState = &_colorBlending;
  pipelineInfo.pDynamicState = &_dynamicState;
  pipelineInfo.layout = _pipelineLayout;
  pipelineInfo.pTessellationState = &tessellationState;
  pipelineInfo.subpass = 0;
  pipelineInfo.renderPass = renderPass->getRenderPass();
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(_device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }
}

void Pipeline::createGraphicTerrain(
    VkCullModeFlags cullMode,
    VkPolygonMode polygonMode,
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
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = descriptorSetLayoutRaw.size();
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayoutRaw.data();

  _inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;

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
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  _rasterizer.cullMode = cullMode;
  _rasterizer.polygonMode = polygonMode;

  _depthStencil.depthTestEnable = VK_TRUE;
  _depthStencil.depthWriteEnable = VK_TRUE;

  VkPipelineTessellationStateCreateInfo tessellationState{};
  tessellationState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
  tessellationState.pNext = nullptr;
  tessellationState.flags = 0;
  tessellationState.patchControlPoints = 4;
  std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(renderPass->getColorAttachmentNumber(),
                                                                    _blendAttachmentState);
  _colorBlending.attachmentCount = blendAttachments.size();
  _colorBlending.pAttachments = blendAttachments.data();

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = shaderStages.size();
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &_inputAssembly;
  pipelineInfo.pViewportState = &_viewportState;
  pipelineInfo.pDepthStencilState = &_depthStencil;
  pipelineInfo.pRasterizationState = &_rasterizer;
  pipelineInfo.pMultisampleState = &_multisampling;
  pipelineInfo.pColorBlendState = &_colorBlending;
  pipelineInfo.pDynamicState = &_dynamicState;
  pipelineInfo.layout = _pipelineLayout;
  pipelineInfo.pTessellationState = &tessellationState;
  pipelineInfo.subpass = 0;
  pipelineInfo.renderPass = renderPass->getRenderPass();
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(_device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }
}

void Pipeline::createGraphic3DShadow(
    VkCullModeFlags cullMode,
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
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = descriptorSetLayoutRaw.size();
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

  // create pipeline
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  _rasterizer.cullMode = cullMode;
  _rasterizer.depthBiasEnable = VK_TRUE;
  _depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  _depthStencil.depthTestEnable = VK_TRUE;
  _depthStencil.depthWriteEnable = VK_TRUE;
  _colorBlending.attachmentCount = 0;

  const VkPipelineRenderingCreateInfoKHR pipelineRender{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
                                                        .depthAttachmentFormat = _settings->getDepthFormat()};

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.pNext = &pipelineRender;
  pipelineInfo.stageCount = shaderStages.size();
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &_inputAssembly;
  pipelineInfo.pViewportState = &_viewportState;
  pipelineInfo.pDepthStencilState = &_depthStencil;
  pipelineInfo.pRasterizationState = &_rasterizer;
  pipelineInfo.pMultisampleState = &_multisampling;
  pipelineInfo.pColorBlendState = &_colorBlending;
  pipelineInfo.pDynamicState = &_dynamicState;
  pipelineInfo.layout = _pipelineLayout;
  pipelineInfo.subpass = 0;
  pipelineInfo.renderPass = renderPass->getRenderPass();
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(_device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }
}

void Pipeline::createGraphic2D(
    VkCullModeFlags cullMode,
    VkPolygonMode polygonMode,
    bool enableAlphaBlending,
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
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = descriptorSetLayoutRaw.size();
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

  // create pipeline
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  _rasterizer.cullMode = cullMode;
  _depthStencil.depthTestEnable = VK_TRUE;
  _depthStencil.depthWriteEnable = VK_TRUE;
  _rasterizer.polygonMode = polygonMode;
  if (enableAlphaBlending == false) _blendAttachmentState.blendEnable = VK_FALSE;
  std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(renderPass->getColorAttachmentNumber(),
                                                                    _blendAttachmentState);
  _colorBlending.attachmentCount = blendAttachments.size();
  _colorBlending.pAttachments = blendAttachments.data();

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = shaderStages.size();
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &_inputAssembly;
  pipelineInfo.pViewportState = &_viewportState;
  pipelineInfo.pDepthStencilState = &_depthStencil;
  pipelineInfo.pRasterizationState = &_rasterizer;
  pipelineInfo.pMultisampleState = &_multisampling;
  pipelineInfo.pColorBlendState = &_colorBlending;
  pipelineInfo.pDynamicState = &_dynamicState;
  pipelineInfo.layout = _pipelineLayout;
  pipelineInfo.subpass = 0;
  pipelineInfo.renderPass = renderPass->getRenderPass();
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(_device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }
}

void Pipeline::createGraphic2DShadow(
    VkCullModeFlags cullMode,
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
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = descriptorSetLayoutRaw.size();
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

  // create pipeline
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  _rasterizer.cullMode = cullMode;
  _rasterizer.depthBiasEnable = VK_TRUE;
  _depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  _depthStencil.depthTestEnable = VK_TRUE;
  _depthStencil.depthWriteEnable = VK_TRUE;
  _colorBlending.attachmentCount = 0;

  const VkPipelineRenderingCreateInfoKHR pipelineRender{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
                                                        .depthAttachmentFormat = _settings->getDepthFormat()};

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.pNext = &pipelineRender;
  pipelineInfo.stageCount = shaderStages.size();
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &_inputAssembly;
  pipelineInfo.pViewportState = &_viewportState;
  pipelineInfo.pDepthStencilState = &_depthStencil;
  pipelineInfo.pRasterizationState = &_rasterizer;
  pipelineInfo.pMultisampleState = &_multisampling;
  pipelineInfo.pColorBlendState = &_colorBlending;
  pipelineInfo.pDynamicState = &_dynamicState;
  pipelineInfo.layout = _pipelineLayout;
  pipelineInfo.subpass = 0;
  pipelineInfo.renderPass = renderPass->getRenderPass();
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(_device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }
}

void Pipeline::createParticleSystemGraphic(
    VkCullModeFlags cullMode,
    VkPolygonMode polygonMode,
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
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = descriptorSetLayoutRaw.size();
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

  // create pipeline
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  _inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

  _rasterizer.cullMode = cullMode;
  _rasterizer.polygonMode = polygonMode;
  _depthStencil.depthTestEnable = VK_TRUE;
  _depthStencil.depthWriteEnable = VK_FALSE;
  std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(renderPass->getColorAttachmentNumber(),
                                                                    _blendAttachmentState);
  _colorBlending.attachmentCount = blendAttachments.size();
  _colorBlending.pAttachments = blendAttachments.data();

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = shaderStages.size();
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &_inputAssembly;
  pipelineInfo.pViewportState = &_viewportState;
  pipelineInfo.pDepthStencilState = &_depthStencil;
  pipelineInfo.pRasterizationState = &_rasterizer;
  pipelineInfo.pMultisampleState = &_multisampling;
  pipelineInfo.pColorBlendState = &_colorBlending;
  pipelineInfo.pDynamicState = &_dynamicState;
  pipelineInfo.layout = _pipelineLayout;
  pipelineInfo.subpass = 0;
  pipelineInfo.renderPass = renderPass->getRenderPass();
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

  if (vkCreateGraphicsPipelines(_device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
  }
}

void Pipeline::createParticleSystemCompute(
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