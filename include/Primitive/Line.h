#pragma once
#include "State.h"
#include "Camera.h"

class Line {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Buffer> _stagingBuffer;
  std::shared_ptr<VertexBuffer<Vertex3D>> _vertexBuffer;
  std::shared_ptr<VertexBuffer<uint32_t>> _indexBuffer;
  std::shared_ptr<UniformBuffer> _uniformBuffer;
  std::shared_ptr<DescriptorSet> _descriptorSetCamera;
  std::shared_ptr<Pipeline> _pipeline;
  std::shared_ptr<Camera> _camera;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;
  glm::mat4 _model = glm::mat4(1.f);
  std::pair<glm::vec3, glm::vec3> _color;
  std::pair<glm::vec3, glm::vec3> _position;
  bool _changed = false;
  std::vector<uint32_t> _indices;

 public:
  Line(int thick,
       VkFormat renderFormat,
       std::shared_ptr<CommandBuffer> commandBufferTransfer,
       std::shared_ptr<State> state);
  void setPosition(glm::vec3 p0, glm::vec3 p1);
  std::pair<glm::vec3, glm::vec3> getPosition();
  void setModel(glm::mat4 model);
  void setColor(glm::vec3 c0, glm::vec3 c1);
  void setCamera(std::shared_ptr<Camera> camera);

  void draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer);
};