module;
export module Skybox;
import "glm/glm.hpp";
import "vulkan/vulkan.hpp";
import <map>;
import <memory>;
import State;
import Cubemap;
import Camera;
import Material;
import Mesh;
import Pipeline;
import ResourceManager;
import Drawable;
import Descriptor;
import Buffer;
import Command;

export namespace VulkanEngine {
class Skybox {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Mesh3D> _mesh;
  std::shared_ptr<UniformBuffer> _uniformBuffer;
  std::shared_ptr<DescriptorSet> _descriptorSetCamera;
  std::shared_ptr<Pipeline> _pipeline;
  std::shared_ptr<Material> _material;
  std::shared_ptr<MaterialColor> _defaultMaterialColor;
  MaterialType _materialType = MaterialType::COLOR;
  glm::mat4 _model = glm::mat4(1.f);

 public:
  Skybox(std::shared_ptr<CommandBuffer> commandBufferTransfer,
         std::shared_ptr<ResourceManager> resourceManager,
         std::shared_ptr<State> state);
  void setMaterial(std::shared_ptr<MaterialColor> material);
  void setModel(glm::mat4 model);
  std::shared_ptr<Mesh3D> getMesh();

  void draw(std::tuple<int, int> resolution,
            std::shared_ptr<Camera> camera,
            std::shared_ptr<CommandBuffer> commandBuffer);
};
}  // namespace VulkanEngine