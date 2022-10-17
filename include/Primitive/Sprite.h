#pragma once
#include "Device.h"
#include "Buffer.h"
#include "Shader.h"
#include "Render.h"
#include "Pipeline.h"
#include "Command.h"
#include "Queue.h"
#include "Settings.h"

class Sprite {
 private:
  std::shared_ptr<Settings> _settings;
  std::shared_ptr<Device> _device;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayout;
  std::shared_ptr<Shader> _shader;
  std::shared_ptr<RenderPass> _render;
  std::shared_ptr<Pipeline> _pipeline;
  std::shared_ptr<CommandPool> _commandPool;
  std::shared_ptr<DescriptorPool> _descriptorPool;
  std::shared_ptr<Queue> _queue;
  std::shared_ptr<DescriptorSet> _descriptorSet;
  std::shared_ptr<CommandBuffer> _commandBuffer;

  std::shared_ptr<VertexBuffer> _vertexBuffer;
  std::shared_ptr<IndexBuffer> _indexBuffer;
  std::shared_ptr<UniformBuffer> _uniformBuffer;

 public:
  Sprite(std::shared_ptr<DescriptorPool> descriptorPool,
         std::shared_ptr<CommandPool> commandPool,
         std::shared_ptr<CommandBuffer> commandBuffer,
         std::shared_ptr<Queue> queue,
         std::shared_ptr<RenderPass> render,
         std::shared_ptr<Device> device,
         std::shared_ptr<Settings> settings);

  void draw(int currentFrame, glm::mat4 model, glm::mat4 view, glm::mat4 proj);
};