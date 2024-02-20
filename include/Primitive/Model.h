#pragma once
#include "Device.h"
#include "Buffer.h"
#include "Shader.h"
#include "Pipeline.h"
#include "Command.h"
#include "Settings.h"
#include "Camera.h"
#include "LightManager.h"
#include "Logger.h"
#include "Loader.h"
#include "Animation.h"
#include "Material.h"
#include "ResourceManager.h"
#include "Drawable.h"

class Model3D : public Drawable, public Shadowable {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<LoggerCPU> _loggerCPU;

  std::vector<std::shared_ptr<NodeGLTF>> _nodes;
  std::vector<std::vector<std::shared_ptr<UniformBuffer>>> _cameraUBODepth;
  std::shared_ptr<UniformBuffer> _cameraUBOFull;
  std::shared_ptr<CommandPool> _commandPool;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;
  std::vector<std::vector<std::shared_ptr<DescriptorSet>>> _descriptorSetCameraDepth;
  std::shared_ptr<DescriptorSet> _descriptorSetCameraFull, _descriptorSetCameraGeometry;
  std::shared_ptr<DescriptorSet> _descriptorSetJointsDefault;
  std::shared_ptr<DescriptorPool> _descriptorPool;
  std::map<MaterialType, std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>>>
      _descriptorSetLayout;
  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> _descriptorSetLayoutNormal;
  std::map<MaterialType, std::shared_ptr<Pipeline>> _pipeline, _pipelineCullOff, _pipelineWireframe;
  std::shared_ptr<Pipeline> _pipelineNormalMesh, _pipelineNormalMeshCullOff, _pipelineTangentMesh,
      _pipelineTangentMeshCullOff;
  std::shared_ptr<Pipeline> _pipelineDirectional, _pipelinePoint;

  std::shared_ptr<MaterialPhong> _defaultMaterialPhong;
  std::shared_ptr<MaterialPBR> _defaultMaterialPBR;
  std::shared_ptr<MaterialColor> _defaultMaterialColor;
  std::shared_ptr<Mesh3D> _mesh;
  std::shared_ptr<Animation> _defaultAnimation;
  // used only for pipeline layout, not used for bind pipeline (layout is the same in every pipeline)
  std::shared_ptr<Texture> _stubTexture;
  std::shared_ptr<Texture> _stubTextureNormal;
  std::vector<std::shared_ptr<Buffer>> _defaultSSBO;
  bool _enableDepth = true;
  int _animationIndex = 0;
  bool _enableShadow = true;
  bool _enableLighting = true;
  std::shared_ptr<LightManager> _lightManager;
  std::shared_ptr<Animation> _animation;
  std::vector<std::shared_ptr<Material>> _materials;
  std::vector<std::shared_ptr<Mesh3D>> _meshes;
  MaterialType _materialType = MaterialType::PHONG;
  DrawType _drawType = DrawType::FILL;

  void _drawNode(std::shared_ptr<CommandBuffer> commandBuffer,
                 std::shared_ptr<Pipeline> pipeline,
                 std::shared_ptr<Pipeline> pipelineCullOff,
                 std::shared_ptr<DescriptorSet> cameraDS,
                 std::shared_ptr<UniformBuffer> cameraUBO,
                 glm::mat4 view,
                 glm::mat4 projection,
                 std::shared_ptr<NodeGLTF> node);

 public:
  Model3D(std::vector<VkFormat> renderFormat,
          const std::vector<std::shared_ptr<NodeGLTF>>& nodes,
          const std::vector<std::shared_ptr<Mesh3D>>& meshes,
          std::shared_ptr<LightManager> lightManager,
          std::shared_ptr<CommandBuffer> commandBufferTransfer,
          std::shared_ptr<ResourceManager> resourceManager,
          std::shared_ptr<State> state);
  void enableShadow(bool enable);
  void enableLighting(bool enable);

  void setMaterial(std::vector<std::shared_ptr<MaterialPBR>> materials);
  void setMaterial(std::vector<std::shared_ptr<MaterialPhong>> materials);
  void setMaterial(std::vector<std::shared_ptr<MaterialColor>> materials);
  void setAnimation(std::shared_ptr<Animation> animation);
  void setDrawType(DrawType drawType);

  void enableDepth(bool enable);
  bool isDepthEnabled();

  MaterialType getMaterialType();
  DrawType getDrawType();

  void draw(std::tuple<int, int> resolution,
            std::shared_ptr<Camera> camera,
            std::shared_ptr<CommandBuffer> commandBuffer) override;
  void drawShadow(LightType lightType, int lightIndex, int face, std::shared_ptr<CommandBuffer> commandBuffer);
};