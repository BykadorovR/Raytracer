#include "Render.h"
#include <array>

RenderPass::RenderPass(std::shared_ptr<Settings> settings, std::shared_ptr<Device> device) {
  _settings = settings;
  _device = device;
}

void RenderPass::initializeGraphic() {
  std::vector<VkAttachmentDescription> attachmentDescriptors(3);
  attachmentDescriptors[0].format = _settings->getGraphicColorFormat();
  attachmentDescriptors[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachmentDescriptors[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachmentDescriptors[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachmentDescriptors[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachmentDescriptors[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachmentDescriptors[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  // goes to bloom (not blur) postprocessing as input image
  attachmentDescriptors[0].finalLayout = VK_IMAGE_LAYOUT_GENERAL;

  attachmentDescriptors[1].format = _settings->getGraphicColorFormat();
  attachmentDescriptors[1].samples = VK_SAMPLE_COUNT_1_BIT;
  attachmentDescriptors[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachmentDescriptors[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachmentDescriptors[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachmentDescriptors[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachmentDescriptors[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  // goes to blur postprocessing stage (bloom happens after blur) and then to bloom as blured image
  attachmentDescriptors[1].finalLayout = VK_IMAGE_LAYOUT_GENERAL;

  attachmentDescriptors[2].format = _settings->getDepthFormat();
  attachmentDescriptors[2].samples = VK_SAMPLE_COUNT_1_BIT;
  attachmentDescriptors[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachmentDescriptors[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachmentDescriptors[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachmentDescriptors[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  // comes from light baking to depth image
  attachmentDescriptors[2].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  // goes to GUI for visualization
  attachmentDescriptors[2].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  std::vector<VkAttachmentReference> colorAttachmentRef(2);
  colorAttachmentRef[0].attachment = 0;
  colorAttachmentRef[0].layout = VK_IMAGE_LAYOUT_GENERAL;
  colorAttachmentRef[1].attachment = 1;
  colorAttachmentRef[1].layout = VK_IMAGE_LAYOUT_GENERAL;

  VkAttachmentReference depthAttachmentRef{};
  depthAttachmentRef.attachment = 2;
  // we want read depth in shader
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = colorAttachmentRef.size();
  subpass.pColorAttachments = colorAttachmentRef.data();
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptors.size());
  renderPassInfo.pAttachments = attachmentDescriptors.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 0;
  renderPassInfo.pDependencies = nullptr;

  if (vkCreateRenderPass(_device->getLogicalDevice(), &renderPassInfo, nullptr, &_renderPass) != VK_SUCCESS) {
    throw std::runtime_error("failed to create render pass!");
  }
}

void RenderPass::initializeDebug() {
  std::vector<VkAttachmentDescription> attachmentDescriptors(1);
  attachmentDescriptors[0].format = _settings->getGraphicColorFormat();
  attachmentDescriptors[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachmentDescriptors[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  attachmentDescriptors[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachmentDescriptors[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachmentDescriptors[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  // comes from postprocessing
  attachmentDescriptors[0].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
  // goes to presentation
  attachmentDescriptors[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef;
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_GENERAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptors.size());
  renderPassInfo.pAttachments = attachmentDescriptors.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 0;
  renderPassInfo.pDependencies = nullptr;

  if (vkCreateRenderPass(_device->getLogicalDevice(), &renderPassInfo, nullptr, &_renderPass) != VK_SUCCESS) {
    throw std::runtime_error("failed to create render pass!");
  }
}

void RenderPass::initializeLightDepth() {
  std::vector<VkAttachmentDescription> attachmentDescriptors(1);
  attachmentDescriptors[0].format = _settings->getGraphicColorFormat();
  attachmentDescriptors[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachmentDescriptors[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachmentDescriptors[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachmentDescriptors[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachmentDescriptors[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  // we want read depth buffer in graphic render later, so after graphic render it's read only (initially also, so it's
  // the same behaviour)
  attachmentDescriptors[0].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  // graphic render will read from it, so change back to read only
  attachmentDescriptors[0].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkAttachmentReference depthAttachmentRef{};
  depthAttachmentRef.attachment = 0;
  // inside shader it should be attachment because we wanna write to it
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 0;
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptors.size());
  renderPassInfo.pAttachments = attachmentDescriptors.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 0;
  renderPassInfo.pDependencies = nullptr;

  if (vkCreateRenderPass(_device->getLogicalDevice(), &renderPassInfo, nullptr, &_renderPass) != VK_SUCCESS) {
    throw std::runtime_error("failed to create render pass!");
  }
}

void RenderPass::initializeIBL() {
  std::vector<VkAttachmentDescription> attachmentDescriptors(1);
  attachmentDescriptors[0].format = _settings->getGraphicColorFormat();
  attachmentDescriptors[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachmentDescriptors[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachmentDescriptors[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachmentDescriptors[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachmentDescriptors[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  //comes from previous stage/initialization
  attachmentDescriptors[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  //goes to render shader as input
  attachmentDescriptors[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentReference colorAttachmentRef;
  colorAttachmentRef.attachment = 0;
  //we want write to this attachments
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptors.size());
  renderPassInfo.pAttachments = attachmentDescriptors.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 0;
  renderPassInfo.pDependencies = nullptr;

  if (vkCreateRenderPass(_device->getLogicalDevice(), &renderPassInfo, nullptr, &_renderPass) != VK_SUCCESS) {
    throw std::runtime_error("failed to create render pass!");
  }
}

VkRenderPassBeginInfo RenderPass::getRenderPassInfo(std::shared_ptr<Framebuffer> framebuffer) {
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = _renderPass;
  renderPassInfo.framebuffer = framebuffer->getBuffer();
  renderPassInfo.renderArea.offset = {0, 0};
  auto [width, height] = framebuffer->getResolution();
  renderPassInfo.renderArea.extent.width = width;
  renderPassInfo.renderArea.extent.height = height;

  return renderPassInfo;
}

VkRenderPass& RenderPass::getRenderPass() { return _renderPass; }

RenderPass::~RenderPass() { vkDestroyRenderPass(_device->getLogicalDevice(), _renderPass, nullptr); }

Framebuffer::Framebuffer(std::vector<std::shared_ptr<ImageView>> input,
                         std::tuple<int, int> renderArea,
                         std::shared_ptr<RenderPass> renderPass,
                         std::shared_ptr<Device> device) {
  _device = device;
  _resolution = renderArea;
  std::vector<VkImageView> attachments;
  for (int i = 0; i < input.size(); i++) {
    attachments.push_back(input[i]->getImageView());
  }
  // depth and image must have equal resolution
  VkFramebufferCreateInfo framebufferInfo{};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass = renderPass->getRenderPass();
  framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  framebufferInfo.pAttachments = attachments.data();
  framebufferInfo.width = std::get<0>(renderArea);
  framebufferInfo.height = std::get<1>(renderArea);
  framebufferInfo.layers = 1;

  if (vkCreateFramebuffer(_device->getLogicalDevice(), &framebufferInfo, nullptr, &_buffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to create framebuffer!");
  }
}

std::tuple<int, int> Framebuffer::getResolution() { return _resolution; }

VkFramebuffer Framebuffer::getBuffer() { return _buffer; }

Framebuffer::~Framebuffer() {
  vkDestroyFramebuffer(_device->getLogicalDevice(), _buffer, nullptr);
}