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
#include "Mesh.h"

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
  std::vector<std::vector<std::shared_ptr<UniformBuffer>>> _cameraUBODepth;
  std::shared_ptr<UniformBuffer> _cameraUBOFull;
  std::shared_ptr<MaterialPhong> _material;
  std::shared_ptr<Mesh2D> _mesh;

  glm::mat4 _model = glm::mat4(1.f);

 public:
  Sprite(std::shared_ptr<Mesh2D> mesh,
         std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
         std::shared_ptr<CommandBuffer> commandBufferTransfer,
         std::shared_ptr<State> state);
  void enableShadow(bool enable);
  void enableLighting(bool enable);
  void enableDepth(bool enable);
  bool isDepthEnabled();

  void setMaterial(std::shared_ptr<MaterialPhong> material);
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