module;
export module Terrain;
import "glm/glm.hpp";
import "vulkan/vulkan.hpp";
import <map>;
import Drawable;
import State;
import Camera;
import LightManager;
import Mesh;
import Material;
import Buffer;
import Descriptor;
import Pipeline;
import Settings;
import Texture;

export namespace VulkanEngine {
class Terrain : public Drawable, public Shadowable {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Mesh3D> _mesh;
  std::shared_ptr<Material> _material;
  MaterialType _materialType = MaterialType::COLOR;

  std::shared_ptr<MaterialColor> _defaultMaterialColor;
  std::shared_ptr<MaterialPhong> _defaultMaterialPhong;
  std::shared_ptr<MaterialPBR> _defaultMaterialPBR;

  std::shared_ptr<UniformBuffer> _cameraBuffer;
  std::vector<std::vector<std::shared_ptr<UniformBuffer>>> _cameraBufferDepth;
  std::vector<std::vector<std::shared_ptr<DescriptorSet>>> _descriptorSetCameraDepthControl,
      _descriptorSetCameraDepthEvaluation;
  std::map<MaterialType, std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>>>
      _descriptorSetLayout;
  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> _descriptorSetLayoutNormalsMesh,
      _descriptorSetLayoutShadows;
  std::shared_ptr<DescriptorSet> _descriptorSetCameraControl, _descriptorSetCameraEvaluation,
      _descriptorSetCameraGeometry, _descriptorSetHeight;
  std::map<MaterialType, std::shared_ptr<Pipeline>> _pipeline, _pipelineWireframe;
  std::shared_ptr<Pipeline> _pipelineDirectional, _pipelinePoint, _pipelineNormalMesh, _pipelineTangentMesh;
  std::shared_ptr<Texture> _heightMap;
  std::pair<int, int> _patchNumber;
  std::shared_ptr<LightManager> _lightManager;
  int _mipMap = 8;
  float _heightScale = 64.f;
  float _heightShift = 16.f;
  float _heightLevels[4] = {16, 128, 192, 256};
  int _minTessellationLevel = 4, _maxTessellationLevel = 32;
  float _minDistance = 30, _maxDistance = 100;
  bool _enableEdge = false;
  bool _showLoD = false;
  bool _enableLighting = true;
  bool _enableShadow = true;
  DrawType _drawType = DrawType::FILL;

 public:
  Terrain(std::shared_ptr<BufferImage> heightMap,
          std::pair<int, int> patchNumber,
          std::vector<VkFormat> renderFormat,
          std::shared_ptr<CommandBuffer> commandBufferTransfer,
          std::shared_ptr<LightManager> lightManager,
          std::shared_ptr<State> state);

  void enableShadow(bool enable);
  void enableLighting(bool enable);
  void setMaterial(std::shared_ptr<MaterialColor> material);
  void setMaterial(std::shared_ptr<MaterialPhong> material);
  void setMaterial(std::shared_ptr<MaterialPBR> material);
  void setDrawType(DrawType drawType);

  DrawType getDrawType();
  void patchEdge(bool enable);
  void showLoD(bool enable);
  void draw(std::tuple<int, int> resolution,
            std::shared_ptr<Camera> camera,
            std::shared_ptr<CommandBuffer> commandBuffer) override;
  void drawShadow(LightType lightType, int lightIndex, int face, std::shared_ptr<CommandBuffer> commandBuffer) override;
};
}  // namespace VulkanEngine