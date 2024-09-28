#pragma once
#include "Utility/State.h"
#include "Utility/ResourceManager.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/Render.h"
#include "Graphic/Camera.h"
#include "Graphic/Material.h"
#include "Primitive/Cubemap.h"
#include "Primitive/Mesh.h"
#include "Primitive/Drawable.h"

class Skybox {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<MeshStatic3D> _mesh;
  std::shared_ptr<UniformBuffer> _uniformBuffer;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayout;
  std::shared_ptr<DescriptorSet> _descriptorSet;
  std::shared_ptr<Pipeline> _pipeline;
  std::shared_ptr<RenderPass> _renderPass;
  std::shared_ptr<Material> _material;
  std::shared_ptr<MaterialColor> _defaultMaterialColor;
  MaterialType _materialType = MaterialType::COLOR;
  glm::mat4 _model = glm::mat4(1.f);

  void _updateColorDescriptor(std::shared_ptr<MaterialColor> material);

 public:
  Skybox(std::shared_ptr<CommandBuffer> commandBufferTransfer,
         std::shared_ptr<ResourceManager> resourceManager,
         std::shared_ptr<State> state);
  void setMaterial(std::shared_ptr<MaterialColor> material);
  void setModel(glm::mat4 model);
  std::shared_ptr<MeshStatic3D> getMesh();

  void draw(std::tuple<int, int> resolution,
            std::shared_ptr<Camera> camera,
            std::shared_ptr<CommandBuffer> commandBuffer);
};