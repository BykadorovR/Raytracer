#pragma once
#include "Device.h"
#include "Render.h"
#include "Texture.h"
#include "Settings.h"
#include "Shader.h"
#include "Descriptor.h"
#include "Pipeline.h"

class ComputePart {
 private:
  std::shared_ptr<Device> _device;
  std::shared_ptr<Queue> _queue;
  std::shared_ptr<CommandPool> _commandPool;
  std::shared_ptr<Settings> _settings;

  std::shared_ptr<RenderPass> _renderPass;
  std::shared_ptr<Framebuffer> _framebuffer;
  std::shared_ptr<Pipeline> _pipeline;
  std::shared_ptr<Shader> _shader;
  std::shared_ptr<DescriptorSet> _descriptorSet;
  std::shared_ptr<CommandBuffer> _commandBuffer;
  std::shared_ptr<DescriptorPool> _descriptorPool;
  std::shared_ptr<UniformBuffer> _uniformBuffer, _uniformBufferSpheres, _uniformBufferRectangles,
      _uniformBufferHitboxes, _uniformBufferSettings;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayout;

  std::vector<std::shared_ptr<Image>> _images;
  std::vector<std::shared_ptr<ImageView>> _imageViews;
  std::vector<std::shared_ptr<Texture>> _resultTextures;
  std::map<std::string, bool*> _checkboxes;

 public:
  ComputePart(std::shared_ptr<Device> device,
              std::shared_ptr<Queue> queue,
              std::shared_ptr<CommandBuffer> commandBuffer,
              std::shared_ptr<CommandPool> commandPool,
              std::shared_ptr<Settings> settings);
  void draw(int currentFrame);
  void recreateResultTextures();

  std::map<std::string, bool*> getCheckboxes();

  std::vector<std::shared_ptr<Texture>> getResultTextures();
  std::shared_ptr<Pipeline> getPipeline();
  std::shared_ptr<DescriptorSet> getDescriptorSet();
};