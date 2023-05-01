#pragma once
#include "Device.h"
#include "Buffer.h"
#include "Shader.h"
#include "Render.h"
#include "Pipeline.h"
#include "Command.h"
#include "Queue.h"
#include "Settings.h"
#include "Camera.h"
#include "LightManager.h"

class Sprite {
 private:
  std::shared_ptr<Settings> _settings;
  std::shared_ptr<Device> _device;
  std::shared_ptr<CommandPool> _commandPool;
  std::vector<std::shared_ptr<DescriptorSet>> _descriptorSet;
  std::shared_ptr<CommandBuffer> _commandBuffer;
  // don't delete, we store references here so it's not deleted
  std::shared_ptr<Texture> _texture, _normalMap;
  std::shared_ptr<Camera> _camera;

  std::shared_ptr<VertexBuffer2D> _vertexBuffer;
  std::shared_ptr<IndexBuffer> _indexBuffer;
  std::shared_ptr<UniformBuffer> _uniformBuffer;

  glm::mat4 _model = glm::mat4(1.f);
  // we swap Y here because image is going from top to bottom, but Vulkan vice versa
  std::vector<Vertex2D> _vertices = {
      {{0.5f, 0.5f, 0.f}, {0.f, 0.f, -1.f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.f, 0.f}},
      {{0.5f, -0.5f, 0.f}, {0.f, 0.f, -1.f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.f, 0.f}},
      {{-0.5f, -0.5f, 0.f}, {0.f, 0.f, -1.f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.f, 0.f}},
      {{-0.5f, 0.5f, 0.f}, {0.f, 0.f, -1.f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.f, 0.f}}};

  const std::vector<uint32_t> _indices = {0, 1, 3, 1, 2, 3};

 public:
  Sprite(std::shared_ptr<Texture> texture,
         std::shared_ptr<Texture> normalMap,
         std::vector<std::shared_ptr<DescriptorSetLayout>> descriptorSetLayout,
         std::shared_ptr<DescriptorPool> descriptorPool,
         std::shared_ptr<CommandPool> commandPool,
         std::shared_ptr<CommandBuffer> commandBuffer,
         std::shared_ptr<Queue> queue,
         std::shared_ptr<Device> device,
         std::shared_ptr<Settings> settings);

  void setModel(glm::mat4 model);
  void setCamera(std::shared_ptr<Camera> camera);
  void setNormal(glm::vec3 normal);
  void draw(int currentFrame, std::shared_ptr<Pipeline> pipeline);
};