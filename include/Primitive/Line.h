#pragma once
#include "Vulkan/Descriptor.h"
#include "Vulkan/Pipeline.h"
#include "Utility/State.h"
#include "Graphic/Camera.h"
#include "Primitive/Mesh.h"
#include "Primitive/Drawable.h"

class Line : public Drawable {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Buffer> _stagingBuffer;
  std::shared_ptr<Mesh3D> _mesh;
  std::shared_ptr<UniformBuffer> _uniformBuffer;
  std::shared_ptr<DescriptorSet> _descriptorSetCamera;
  std::shared_ptr<Pipeline> _pipeline;
  std::shared_ptr<RenderPass> _renderPass;
  glm::mat4 _model = glm::mat4(1.f);
  bool _changed = false;

 public:
  Line(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state);
  std::shared_ptr<Mesh3D> getMesh();
  void setModel(glm::mat4 model);

  void draw(std::tuple<int, int> resolution,
            std::shared_ptr<Camera> camera,
            std::shared_ptr<CommandBuffer> commandBuffer) override;
};