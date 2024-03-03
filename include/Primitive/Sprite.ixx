module;
export module Sprite;
import "vulkan/vulkan.hpp";
import <map>;
import Device;
import State;
import Buffer;
import Shader;
import Pipeline;
import Command;
import Settings;
import Camera;
import LightManager;
import Light;
import Material;
import Mesh;
import ResourceManager;
import Drawable;
import Descriptor;

export namespace VulkanEngine {
class Sprite : public Drawable, public Shadowable {
 private:
  std::shared_ptr<State> _state;

  std::shared_ptr<DescriptorSet> _descriptorSetCameraFull, _descriptorSetCameraGeometry;
  std::vector<std::vector<std::shared_ptr<DescriptorSet>>> _descriptorSetCameraDepth;
  std::map<MaterialType, std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>>>
      _descriptorSetLayout, _descriptorSetLayoutDepth;
  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> _descriptorSetLayoutNormal;
  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> _descriptorSetLayoutBRDF;
  std::map<MaterialType, std::shared_ptr<Pipeline>> _pipeline;
  std::map<MaterialType, std::shared_ptr<Pipeline>> _pipelineWireframe;
  std::shared_ptr<Pipeline> _pipelineNormal, _pipelineTangent;
  std::map<MaterialType, std::shared_ptr<Pipeline>> _pipelineDirectional, _pipelinePoint;
  std::shared_ptr<LightManager> _lightManager;

  bool _enableShadow = true;
  bool _enableLighting = true;
  bool _enableDepth = true;
  bool _enableHUD = false;
  std::vector<std::vector<std::shared_ptr<UniformBuffer>>> _cameraUBODepth;
  std::shared_ptr<UniformBuffer> _cameraUBOFull;
  std::shared_ptr<Material> _material;
  std::shared_ptr<MaterialPhong> _defaultMaterialPhong;
  std::shared_ptr<MaterialPBR> _defaultMaterialPBR;
  std::shared_ptr<MaterialColor> _defaultMaterialColor;
  std::shared_ptr<Mesh2D> _mesh;
  MaterialType _materialType = MaterialType::PHONG;
  DrawType _drawType = DrawType::FILL;

 public:
  Sprite(std::vector<VkFormat> renderFormat,
         std::shared_ptr<LightManager> lightManager,
         std::shared_ptr<CommandBuffer> commandBufferTransfer,
         std::shared_ptr<ResourceManager> resourceManager,
         std::shared_ptr<State> state);
  void enableShadow(bool enable);
  void enableLighting(bool enable);
  void enableDepth(bool enable);
  void enableHUD(bool enable);
  bool isDepthEnabled();

  void setMaterial(std::shared_ptr<MaterialPBR> material);
  void setMaterial(std::shared_ptr<MaterialPhong> material);
  void setMaterial(std::shared_ptr<MaterialColor> material);
  MaterialType getMaterialType();
  void setDrawType(DrawType drawType);
  DrawType getDrawType();

  void draw(std::tuple<int, int> resolution,
            std::shared_ptr<Camera> camera,
            std::shared_ptr<CommandBuffer> commandBuffer) override;
  void drawShadow(LightType lightType, int lightIndex, int face, std::shared_ptr<CommandBuffer> commandBuffer) override;
};
}  // namespace VulkanEngine