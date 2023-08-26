#pragma once
#include "State.h"
#include "Camera.h"

class Sphere {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<VertexBuffer<Vertex3D>> _vertexBuffer;
  std::shared_ptr<VertexBuffer<uint32_t>> _indexBuffer;
  std::shared_ptr<UniformBuffer> _uniformBuffer;
  std::shared_ptr<DescriptorSet> _descriptorSetCamera;
  std::shared_ptr<Pipeline> _pipeline;
  std::shared_ptr<Camera> _camera;
  glm::mat4 _model = glm::mat4(1.f);
  std::vector<uint32_t> _indices;

 public:
  Sphere(VkFormat renderFormat, std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state);
  void setModel(glm::mat4 model);
  void setCamera(std::shared_ptr<Camera> camera);

  void draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer);
};