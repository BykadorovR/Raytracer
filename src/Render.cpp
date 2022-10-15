#include "Render.h"
#include <array>

VkFormat RenderPass::_findSupportedFormat(const std::vector<VkFormat>& candidates,
                                          VkImageTiling tiling,
                                          VkFormatFeatureFlags features) {
  for (VkFormat format : candidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(_device->getPhysicalDevice(), format, &props);

    if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
      return format;
    } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  throw std::runtime_error("failed to find supported format!");
}

RenderPass::RenderPass(std::shared_ptr<Swapchain> swapchain, std::shared_ptr<Device> device) {
  _device = device;

  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = swapchain->getImageFormat();
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcAccessMask = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  if (vkCreateRenderPass(device->getLogicalDevice(), &renderPassInfo, nullptr, &_renderPass) != VK_SUCCESS) {
    throw std::runtime_error("failed to create render pass!");
  }
}

VkRenderPass& RenderPass::getRenderPass() { return _renderPass; }

RenderPass::~RenderPass() { vkDestroyRenderPass(_device->getLogicalDevice(), _renderPass, nullptr); }

Framebuffer::Framebuffer(std::tuple<int, int> resolution,
                         std::shared_ptr<Swapchain> swapchain,
                         std::shared_ptr<RenderPass> renderPass,
                         std::shared_ptr<Device> device) {
  _device = device;
  auto imageViews = swapchain->getImageViews();
  _buffer.resize(imageViews.size());

  for (int i = 0; i < _buffer.size(); i++) {
    VkImageView attachments[] = {imageViews[i]->getImageView()};

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass->getRenderPass();
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = std::get<0>(resolution);
    framebufferInfo.height = std::get<1>(resolution);
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(device->getLogicalDevice(), &framebufferInfo, nullptr, &_buffer[i]) != VK_SUCCESS) {
      throw std::runtime_error("failed to create framebuffer!");
    }
  }
}

std::vector<VkFramebuffer>& Framebuffer::getBuffer() { return _buffer; }

Framebuffer::~Framebuffer() {
  for (auto& buffer : _buffer) vkDestroyFramebuffer(_device->getLogicalDevice(), buffer, nullptr);
}