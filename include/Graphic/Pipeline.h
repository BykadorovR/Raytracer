#pragma once
#include "Device.h"
#include "Shader.h"
#include "Buffer.h"
#include "Render.h"
#include "Descriptor.h"

class Pipeline {
 private:
  std::shared_ptr<Device> _device;
  VkPipeline _pipeline;
  VkPipelineLayout _pipelineLayout;

 public:
  Pipeline(std::shared_ptr<Shader> shader,
           std::shared_ptr<DescriptorSetLayout> descriptorSetLayout,
           std::shared_ptr<RenderPass> renderPass,
           std::shared_ptr<Device> device);
  VkPipeline& getPipeline();
  VkPipelineLayout& getPipelineLayout();
  ~Pipeline();
};