#pragma once
#include "Utility/EngineState.h"
#include "Utility/GameState.h"
#include "Graphic/Camera.h"
#include "Graphic/Material.h"
#include "Graphic/LightManager.h"
#include "Primitive/Drawable.h"
#include "Primitive/Line.h"
#include "Primitive/Mesh.h"
#include "Utility/PhysicsManager.h"
#include <optional>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include "Utility/GUI.h"
#include "Primitive/Shape3D.h"
#include "Primitive/Terrain.h"

class TerrainInterpolationDebug : public TerrainDebug {
 private:
  std::shared_ptr<MaterialColor> _defaultMaterialColor;
  std::shared_ptr<GUI> _gui;
  std::vector<std::shared_ptr<Buffer>> _cameraBuffer;
  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> _descriptorSetLayout;
  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> _descriptorSetLayoutNormalsMesh;
  std::shared_ptr<DescriptorSet> _descriptorSetNormal;
  std::shared_ptr<PipelineGraphic> _pipeline, _pipelineWireframe;
  std::shared_ptr<PipelineGraphic> _pipelineNormalMesh, _pipelineTangentMesh;
  std::shared_ptr<RenderPass> _renderPass;
  int _pickedTile = -1;
  float _stripeLeft = 0.1f, _stripeRight = 0.3f, _stripeTop = 0.2f, _stripeBot = 0.4f;
  glm::ivec2 _pickedPixel = glm::ivec2(-1, -1);
  bool _showWireframe = false, _showNormals = false, _showPatches = false;
  glm::vec2 _cursorPosition = glm::vec2(-1.f);
  glm::vec2 _clickPosition = glm::vec2(-1.f);

  int _angleIndex = -1, _textureIndex = -1;
  std::vector<std::shared_ptr<Buffer>> _patchDescriptionSSBO;
  char _terrainPath[256] = "";

  void _updateColorDescriptor();
  void _reallocatePatchDescription(int currentFrame);
  void _updatePatchDescription(int currentFrame);
  int _saveAuxilary(std::string path);
  void _loadAuxilary(std::string path);

 public:
  TerrainInterpolationDebug(std::shared_ptr<ImageCPU<uint8_t>> heightMapCPU,
                            std::pair<int, int> patchNumber,
                            std::shared_ptr<CommandBuffer> commandBufferTransfer,
                            std::shared_ptr<GUI> gui,
                            std::shared_ptr<GameState> gameState,
                            std::shared_ptr<EngineState> engineState);

  void draw(std::shared_ptr<CommandBuffer> commandBuffer) override;
  void drawDebug(std::shared_ptr<CommandBuffer> commandBuffer) override;

  void cursorNotify(float xPos, float yPos) override;
  void mouseNotify(int button, int action, int mods) override;
  void keyNotify(int key, int scancode, int action, int mods) override;
  void charNotify(unsigned int code) override;
  void scrollNotify(double xOffset, double yOffset) override;
  virtual ~TerrainInterpolationDebug() = default;
};

class TerrainInterpolation : public TerrainGPU {
 private:
  std::shared_ptr<GameState> _gameState;

  std::shared_ptr<MaterialColor> _defaultMaterialColor;

  std::vector<std::shared_ptr<Buffer>> _cameraBuffer;
  std::vector<std::vector<std::vector<std::shared_ptr<Buffer>>>> _cameraBufferDepth;
  std::vector<std::vector<std::shared_ptr<DescriptorSet>>> _descriptorSetCameraDepth;
  std::map<MaterialType, std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>>>
      _descriptorSetLayout;
  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> _descriptorSetLayoutShadows;
  std::map<MaterialType, std::shared_ptr<PipelineGraphic>> _pipeline;
  std::shared_ptr<RenderPass> _renderPass, _renderPassShadow;
  std::shared_ptr<PipelineGraphic> _pipelineDirectional, _pipelinePoint;

  std::vector<std::shared_ptr<Buffer>> _patchDescriptionSSBO;
  float _stripeLeft = 0.1f, _stripeRight = 0.3f, _stripeTop = 0.2f, _stripeBot = 0.4f;

  void _fillPatchTexturesDescription();
  void _fillPatchRotationsDescription();
  void _allocatePatchDescription(int currentFrame);
  void _updatePatchDescription(int currentFrame);
  void _updateColorDescriptor();
  void _updatePhongDescriptor();
  void _updatePBRDescriptor();

 public:
  TerrainInterpolation(std::shared_ptr<ImageCPU<uint8_t>> heightMapCPU,
                       std::shared_ptr<GameState> gameState,
                       std::shared_ptr<EngineState> engineState);
  void initialize(std::shared_ptr<CommandBuffer> commandBuffer) override;
  void setStripes(float stripeLeft, float stripeTop, float stripeRight, float stripeBot);
  void draw(std::shared_ptr<CommandBuffer> commandBuffer) override;
  void drawShadow(LightType lightType, int lightIndex, int face, std::shared_ptr<CommandBuffer> commandBuffer) override;
  virtual ~TerrainInterpolation() = default;
};