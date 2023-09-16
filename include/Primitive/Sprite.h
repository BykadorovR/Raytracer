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
#include "Material.h"

enum class SpriteRenderMode { DIRECTIONAL, POINT, FULL };

class Sprite {
 private:
  std::shared_ptr<State> _state;

  std::shared_ptr<DescriptorSet> _descriptorSetCameraFull;
  std::vector<std::vector<std::shared_ptr<DescriptorSet>>> _descriptorSetCameraDepth;
  std::shared_ptr<Camera> _camera;
  bool _enableShadow = true;
  bool _enableLighting = true;
  bool _enableDepth = true;
  std::shared_ptr<VertexBuffer<Vertex2D>> _vertexBuffer;
  std::shared_ptr<VertexBuffer<uint32_t>> _indexBuffer;
  std::vector<std::vector<std::shared_ptr<UniformBuffer>>> _cameraUBODepth;
  std::shared_ptr<UniformBuffer> _cameraUBOFull;
  std::shared_ptr<MaterialSpritePhong> _material;

  glm::mat4 _model = glm::mat4(1.f);
  // Vulkan image origin (0,0) is left-top corner
  std::vector<Vertex2D> _vertices = {
      {{0.5f, 0.5f, 0.f}, {0.f, 0.f, -1.f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.f, 0.f}},
      {{0.5f, -0.5f, 0.f}, {0.f, 0.f, -1.f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.f, 0.f}},
      {{-0.5f, -0.5f, 0.f}, {0.f, 0.f, -1.f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.f, 0.f}},
      {{-0.5f, 0.5f, 0.f}, {0.f, 0.f, -1.f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.f, 0.f}}};

  const std::vector<uint32_t> _indices = {0, 1, 3, 1, 2, 3};

 public:
  Sprite(std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
         std::shared_ptr<CommandBuffer> commandBufferTransfer,
         std::shared_ptr<State> state);
  void enableShadow(bool enable);
  void enableLighting(bool enable);
  void enableDepth(bool enable);
  bool isDepthEnabled();

  void setVertexNormal(glm::vec3 normal);
  void setMaterial(std::shared_ptr<MaterialSpritePhong> material);
  void setModel(glm::mat4 model);
  void setCamera(std::shared_ptr<Camera> camera);

  void draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer, std::shared_ptr<Pipeline> pipeline);
  void drawShadow(int currentFrame,
                  std::shared_ptr<CommandBuffer> commandBuffer,
                  std::shared_ptr<Pipeline> pipeline,
                  int lightIndex,
                  glm::mat4 view,
                  glm::mat4 projection,
                  int face);
};