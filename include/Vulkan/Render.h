#pragma once
#include <Vulkan/Device.h>
#include <Utility/Settings.h>

class RenderPass {
 private:
  std::shared_ptr<Device> _device;
  std::shared_ptr<Settings> _settings;
  VkRenderPass _renderPass;
  int _colorAttachmentNumber = 0;

 public:
  RenderPass(std::shared_ptr<Device> device);
  void initializeCustom(std::vector<VkAttachmentDescription> colorDescription,
                        std::vector<VkAttachmentReference> colorReference,
                        std::optional<VkAttachmentReference> depthReference);
  VkRenderPass& getRenderPass();
  int getColorAttachmentNumber();
  ~RenderPass();
};

enum class RenderPassScenario { GRAPHIC, GUI, SHADOW, IBL, BLUR };

class RenderPassManager {
 private:
  std::map<RenderPassScenario, std::shared_ptr<RenderPass>> _renderPasses;

 public:
  RenderPassManager(std::shared_ptr<Settings> settings, std::shared_ptr<Device> device);
  std::shared_ptr<RenderPass> getRenderPass(RenderPassScenario scenario);
};