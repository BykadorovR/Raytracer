#pragma once
#include "Texture.h"

class Framebuffer;

class RenderPass {
 private:
  std::shared_ptr<Device> _device;
  std::shared_ptr<Settings> _settings;
  VkRenderPass _renderPass;
  int _colorAttachmentNumber = 0;

 public:
  RenderPass(std::shared_ptr<Settings> settings, std::shared_ptr<Device> device);
  void initializeGraphic();
  void initializeDebug();
  void initializeLightDepth();
  void initializeIBL();
  VkRenderPassBeginInfo getRenderPassInfo(std::shared_ptr<Framebuffer> framebuffer);
  VkRenderPass& getRenderPass();
  int getColorAttachmentNumber();
  ~RenderPass();
};

class Framebuffer {
 private:
  std::shared_ptr<Device> _device;
  VkFramebuffer _buffer;
  std::tuple<int, int> _resolution;

 public:
  Framebuffer(std::vector<std::shared_ptr<ImageView>> input,
              std::tuple<int, int> renderArea,
              std::shared_ptr<RenderPass> renderPass,
              std::shared_ptr<Device> device);

  std::tuple<int, int> getResolution();
  VkFramebuffer getBuffer();
  ~Framebuffer();
};
