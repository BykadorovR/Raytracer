#pragma once
#include "Device.h"
#include "Buffer.h"
#include "Shader.h"
#include "Pipeline.h"
#include "Command.h"
#include "Settings.h"
#include "Camera.h"
#include "LightManager.h"
#include "Light.h"

enum class SpriteRenderMode { DIRECTIONAL, POINT, FULL };

class Sprite {
 private:
  std::shared_ptr<Settings> _settings;
  std::shared_ptr<Device> _device;

  std::shared_ptr<DescriptorSet> _descriptorSetCameraFull;
  std::vector<std::vector<std::shared_ptr<DescriptorSet>>> _descriptorSetCameraDepth;
  std::vector<std::shared_ptr<DescriptorSet>> _descriptorSetTextures;

  // don't delete, we store references here so it's not deleted
  std::shared_ptr<Texture> _texture, _normalMap;
  std::shared_ptr<Camera> _camera;
  bool _enableShadow = true;
  bool _enableLighting = true;
  bool _enableDepth = true;
  std::shared_ptr<VertexBuffer<Vertex2D>> _vertexBuffer;
  std::shared_ptr<VertexBuffer<uint32_t>> _indexBuffer;
  std::vector<std::vector<std::shared_ptr<UniformBuffer>>> _uniformBufferDepth;
  std::shared_ptr<UniformBuffer> _uniformBufferFull;

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
         std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
         std::shared_ptr<DescriptorPool> descriptorPool,
         std::shared_ptr<CommandBuffer> commandBufferTransfer,
         std::shared_ptr<Device> device,
         std::shared_ptr<Settings> settings);
  void enableShadow(bool enable);
  void enableLighting(bool enable);
  void enableDepth(bool enable);
  bool isDepthEnabled();

  void setColor(glm::vec3 color);
  void setModel(glm::mat4 model);
  void setCamera(std::shared_ptr<Camera> camera);
  void setNormal(glm::vec3 normal);
  void draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer, std::shared_ptr<Pipeline> pipeline);
  void drawShadow(int currentFrame,
                  std::shared_ptr<CommandBuffer> commandBuffer,
                  std::shared_ptr<Pipeline> pipeline,
                  int lightIndex,
                  glm::mat4 view,
                  glm::mat4 projection,
                  int face);
};