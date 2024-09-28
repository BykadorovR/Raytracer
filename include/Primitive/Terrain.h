#pragma once
#include "Utility/State.h"
#include "Graphic/Camera.h"
#include "Graphic/Material.h"
#include "Graphic/LightManager.h"
#include "Primitive/Drawable.h"
#include "Primitive/Mesh.h"
#include "Utility/PhysicsManager.h"
#include <Jolt/Physics/Body/BodyCreationSettings.h>

class TerrainPhysics {
 private:
  std::shared_ptr<PhysicsManager> _physicsManager;
  std::vector<float> _terrainPhysic;
  std::tuple<int, int> _resolution;
  // destructor is private, can't use smart pointer
  JPH::Body* _terrainBody;
  std::vector<float> _heights;
  glm::vec3 _position;

 public:
  TerrainPhysics(std::shared_ptr<ImageCPU<uint8_t>> heightmap,
                 std::tuple<int, int> heightScaleOffset,
                 std::shared_ptr<PhysicsManager> physicsManager);
  void setPosition(glm::vec3 position);
  glm::vec3 getPosition();
  std::tuple<int, int> getResolution();
  std::vector<float> getHeights();
  ~TerrainPhysics();
};

class TerrainCPU : public Drawable {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<MeshStatic3D> _mesh;

  std::shared_ptr<UniformBuffer> _cameraBuffer;
  std::vector<std::vector<std::shared_ptr<UniformBuffer>>> _cameraBufferDepth;
  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> _descriptorSetLayout;
  std::shared_ptr<DescriptorSet> _descriptorSetColor;
  std::shared_ptr<Pipeline> _pipeline, _pipelineWireframe;
  std::shared_ptr<RenderPass> _renderPass;
  std::pair<int, int> _patchNumber;
  float _heightScale = 64.f;
  float _heightShift = 16.f;
  bool _enableEdge = false;
  DrawType _drawType = DrawType::FILL;
  int _numStrips, _numVertsPerStrip;
  bool _hasIndexes = false;

  void _updateColorDescriptor();
  void _loadStrip(std::shared_ptr<ImageCPU<uint8_t>> heightMap,
                  std::shared_ptr<CommandBuffer> commandBufferTransfer,
                  std::shared_ptr<State> state);
  void _loadTriangles(std::vector<float> heights,
                      std::tuple<int, int> resolution,
                      std::shared_ptr<CommandBuffer> commandBufferTransfer,
                      std::shared_ptr<State> state);
  void _loadTerrain(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state);

 public:
  TerrainCPU(std::shared_ptr<ImageCPU<uint8_t>> heightMap,
             std::pair<int, int> patchNumber,
             std::shared_ptr<CommandBuffer> commandBufferTransfer,
             std::shared_ptr<State> state);
  TerrainCPU(std::vector<float> heights,
             std::tuple<int, int> resolution,
             std::shared_ptr<CommandBuffer> commandBufferTransfer,
             std::shared_ptr<State> state);

  void setDrawType(DrawType drawType);

  DrawType getDrawType();
  void patchEdge(bool enable);
  void draw(std::tuple<int, int> resolution,
            std::shared_ptr<Camera> camera,
            std::shared_ptr<CommandBuffer> commandBuffer) override;
};

class Terrain : public Drawable, public Shadowable {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<MeshStatic3D> _mesh;
  std::shared_ptr<Material> _material;
  MaterialType _materialType = MaterialType::COLOR;

  std::shared_ptr<MaterialColor> _defaultMaterialColor;
  std::shared_ptr<MaterialPhong> _defaultMaterialPhong;
  std::shared_ptr<MaterialPBR> _defaultMaterialPBR;

  std::shared_ptr<UniformBuffer> _cameraBuffer;
  std::vector<std::vector<std::shared_ptr<UniformBuffer>>> _cameraBufferDepth;
  std::vector<std::vector<std::shared_ptr<DescriptorSet>>> _descriptorSetCameraDepth;
  std::map<MaterialType, std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>>>
      _descriptorSetLayout;
  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> _descriptorSetLayoutNormalsMesh,
      _descriptorSetLayoutShadows;
  std::shared_ptr<DescriptorSet> _descriptorSetColor, _descriptorSetPhong, _descriptorSetPBR, _descriptorSetNormal;
  std::map<MaterialType, std::shared_ptr<Pipeline>> _pipeline, _pipelineWireframe;
  std::shared_ptr<RenderPass> _renderPass, _renderPassShadow;
  std::shared_ptr<Pipeline> _pipelineDirectional, _pipelinePoint, _pipelineNormalMesh, _pipelineTangentMesh;
  std::shared_ptr<Texture> _heightMap;
  std::pair<int, int> _patchNumber;
  std::shared_ptr<LightManager> _lightManager;
  float _heightScale = 64.f;
  float _heightShift = 16.f;
  std::array<float, 4> _heightLevels = {16, 128, 192, 256};
  int _minTessellationLevel = 4, _maxTessellationLevel = 32;
  float _minDistance = 30, _maxDistance = 100;
  bool _enableEdge = false;
  bool _showLoD = false;
  bool _enableLighting = true;
  bool _enableShadow = true;
  DrawType _drawType = DrawType::FILL;

  void _updateColorDescriptor(std::shared_ptr<MaterialColor> material);
  void _updatePhongDescriptor(std::shared_ptr<MaterialPhong> material);
  void _updatePBRDescriptor(std::shared_ptr<MaterialPBR> material);

 public:
  Terrain(std::shared_ptr<BufferImage> heightMap,
          std::pair<int, int> patchNumber,
          std::shared_ptr<CommandBuffer> commandBufferTransfer,
          std::shared_ptr<LightManager> lightManager,
          std::shared_ptr<State> state);

  void setTessellationLevel(int min, int max);
  void setDisplayDistance(int min, int max);
  void setColorHeightLevels(std::array<float, 4> levels);
  void setHeight(float scale, float shift);

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