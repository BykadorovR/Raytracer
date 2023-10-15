#pragma once
#include "State.h"
#include "Camera.h"
#include "LightManager.h"
#include "Mesh.h"
#include "Material.h"

class Terrain {
 protected:
  std::shared_ptr<Camera> _camera;
  glm::mat4 _model = glm::mat4(1.f);

 public:
  void setModel(glm::mat4 model);
  void setCamera(std::shared_ptr<Camera> camera);

  virtual void draw(int currentFrame,
                    std::shared_ptr<CommandBuffer> commandBuffer,
                    DrawType terrainType = DrawType::FILL) = 0;

  virtual void drawShadow(int currentFrame,
                          std::shared_ptr<CommandBuffer> commandBuffer,
                          LightType lightType,
                          int lightIndex,
                          int face = 0) = 0;
};

enum class TerrainRenderMode { DIRECTIONAL, POINT, FULL };

class TerrainGPU : public Terrain {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Mesh3D> _mesh;
  std::shared_ptr<MaterialPhong> _defaultMaterial;
  std::shared_ptr<UniformBuffer> _cameraBuffer;
  std::vector<std::vector<std::shared_ptr<UniformBuffer>>> _cameraBufferDepth;
  std::vector<std::vector<std::shared_ptr<DescriptorSet>>> _descriptorSetCameraDepthControl,
      _descriptorSetCameraDepthEvaluation;
  std::shared_ptr<DescriptorSet> _descriptorSetCameraControl, _descriptorSetCameraEvaluation,
      _descriptorSetCameraGeometry, _descriptorSetHeight, _descriptorSetTerrainTiles;
  std::shared_ptr<Pipeline> _pipelineWireframe, _pipelineNormal;
  std::map<TerrainRenderMode, std::shared_ptr<Pipeline>> _pipeline;
  std::shared_ptr<Texture> _heightMap;
  std::array<std::shared_ptr<Texture>, 4> _terrainTiles;
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

 public:
  TerrainGPU(std::pair<int, int> patchNumber,
             std::vector<VkFormat> renderFormat,
             std::shared_ptr<CommandBuffer> commandBufferTransfer,
             std::shared_ptr<LightManager> lightManager,
             std::shared_ptr<State> state);
  void patchEdge(bool enable);
  void showLoD(bool enable);
  void draw(int currentFrame,
            std::shared_ptr<CommandBuffer> commandBuffer,
            DrawType terrainType = DrawType::FILL) override;
  void drawShadow(int currentFrame,
                  std::shared_ptr<CommandBuffer> commandBuffer,
                  LightType lightType,
                  int lightIndex,
                  int face = 0) override;
};