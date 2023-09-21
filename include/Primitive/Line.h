#pragma once
#include "State.h"
#include "Camera.h"
#include "Mesh.h"

class Line {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Buffer> _stagingBuffer;
  std::shared_ptr<Mesh3D> _mesh;
  std::shared_ptr<UniformBuffer> _uniformBuffer;
  std::shared_ptr<DescriptorSet> _descriptorSetCamera;
  std::shared_ptr<Pipeline> _pipeline;
  std::shared_ptr<Camera> _camera;
  glm::mat4 _model = glm::mat4(1.f);
  bool _changed = false;

 public:
  Line(int thick,
       std::vector<VkFormat> renderFormat,
       std::shared_ptr<CommandBuffer> commandBufferTransfer,
       std::shared_ptr<State> state);
  std::shared_ptr<Mesh3D> getMesh();
  void setModel(glm::mat4 model);
  void setCamera(std::shared_ptr<Camera> camera);

  void draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer);
};