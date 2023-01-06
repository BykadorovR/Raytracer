#include "OffscreenPart.h"

OffscreenPart::OffscreenPart(std::shared_ptr<Device> device,
                             std::shared_ptr<Queue> queue,
                             std::shared_ptr<CommandPool> commandPool,
                             std::shared_ptr<CommandBuffer> commandBuffer,
                             std::shared_ptr<Settings> settings) {
  _device = device;
  _queue = queue;
  _commandPool = commandPool;
  _commandBuffer = commandBuffer;
  _settings = settings;

  _renderPass = std::make_shared<RenderPass>(VK_FORMAT_R8G8B8A8_UNORM, device);
  _renderPass->initializeOffscreen();

  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    std::shared_ptr<Image> image = std::make_shared<Image>(
        settings->getResolution(), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, device);
    image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandPool, queue);
    std::shared_ptr<ImageView> imageView = std::make_shared<ImageView>(image, VK_IMAGE_ASPECT_COLOR_BIT, device);
    _resultImageViews.push_back(imageView);
    std::shared_ptr<Texture> texture = std::make_shared<Texture>(imageView, device);
    _resultTextures.push_back(texture);
  }

  std::shared_ptr<Image> image = std::make_shared<Image>(
      settings->getResolution(), VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, device);
  _resultDepth = std::make_shared<ImageView>(image, VK_IMAGE_ASPECT_DEPTH_BIT, device);

  _framebuffer = std::make_shared<Framebuffer>(settings->getResolution(), _resultImageViews, _resultDepth, _renderPass,
                                               device);
}

std::shared_ptr<Framebuffer> OffscreenPart::getFramebuffer() { return _framebuffer; }

std::shared_ptr<RenderPass> OffscreenPart::getRenderPass() { return _renderPass; }

std::vector<std::shared_ptr<Texture>> OffscreenPart::getResultTextures() { return _resultTextures; }