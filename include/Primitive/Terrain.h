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

class TerrainPhysics {
 private:
  std::shared_ptr<PhysicsManager> _physicsManager;
  std::vector<float> _terrainPhysic;
  std::tuple<int, int> _resolution;
  std::vector<float> _heights;
  glm::vec3 _position;
  glm::vec3 _scale;
  JPH::BodyID _terrainID;
  JPH::Ref<JPH::ScaledShape> _terrainShape;
  std::tuple<int, int> _heightScaleOffset;
  std::shared_ptr<ImageCPU<uint8_t>> _heightmap;
  void _initialize();

 public:
  TerrainPhysics(std::shared_ptr<ImageCPU<uint8_t>> heightmap,
                 glm::vec3 position,
                 glm::vec3 scale,
                 std::tuple<int, int> heightScaleOffset,
                 std::shared_ptr<PhysicsManager> physicsManager);
  void reset(std::shared_ptr<ImageCPU<uint8_t>> heightmap);
  void setPosition(glm::vec3 position);
  glm::vec3 getPosition();
  std::tuple<int, int> getResolution();
  std::vector<float> getHeights();
  std::optional<glm::vec3> hit(glm::vec3 origin, glm::vec3 direction);
  ~TerrainPhysics();
};

class TerrainCPU : public Drawable {
 private:
  std::shared_ptr<EngineState> _engineState;
  std::shared_ptr<GameState> _gameState;
  std::tuple<int, int> _resolution;
  std::vector<std::shared_ptr<MeshDynamic3D>> _mesh;
  std::vector<bool> _changeMeshTriangles, _changeMeshHeightmap;
  std::shared_ptr<UniformBuffer> _cameraBuffer;
  std::vector<std::vector<std::shared_ptr<UniformBuffer>>> _cameraBufferDepth;
  std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> _descriptorSetLayout;
  std::shared_ptr<DescriptorSet> _descriptorSetColor;
  std::shared_ptr<Pipeline> _pipeline, _pipelineWireframe;
  std::shared_ptr<RenderPass> _renderPass;
  float _heightScale = 64.f;
  float _heightShift = 16.f;
  bool _enableEdge = false;
  DrawType _drawType = DrawType::FILL;
  int _numStrips, _numVertsPerStrip;
  bool _hasIndexes = false;
  std::shared_ptr<ImageCPU<uint8_t>> _heightMap;
  std::vector<float> _heights;

  void _updateColorDescriptor();
  void _loadStrip(int currentFrame);
  void _loadTriangles(int currentFrame);
  void _loadTerrain();

 public:
  TerrainCPU(std::shared_ptr<ImageCPU<uint8_t>> heightMap,
             std::shared_ptr<GameState> gameState,
             std::shared_ptr<EngineState> engineState);
  TerrainCPU(std::vector<float> heights,
             std::tuple<int, int> resolution,
             std::shared_ptr<GameState> gameState,
             std::shared_ptr<EngineState> engineState);

  void setDrawType(DrawType drawType);
  void setHeightmap(std::shared_ptr<ImageCPU<uint8_t>> heightMap);
  void setHeightmap(std::vector<float> heights);

  DrawType getDrawType();
  void patchEdge(bool enable);
  void draw(std::shared_ptr<CommandBuffer> commandBuffer) override;
};

struct alignas(16) PatchDescription {
  int rotation;
  int textureID;
};

class TerrainDebug : public Drawable, public InputSubscriber {
 protected:
  std::shared_ptr<EngineState> _engineState;
  std::shared_ptr<GameState> _gameState;
  std::shared_ptr<TerrainPhysics> _terrainPhysics;
  std::shared_ptr<DescriptorSet> _descriptorSetColor;

  std::shared_ptr<Material> _material;
  std::vector<bool> _changedMaterial;
  std::shared_ptr<ImageCPU<uint8_t>> _heightMapCPU;
  std::shared_ptr<BufferImage> _heightMapGPU;
  std::shared_ptr<Texture> _heightMap;
  std::vector<std::shared_ptr<MeshDynamic3D>> _mesh;

  DrawType _drawType = DrawType::FILL;
  float _heightScale = 64.f;
  float _heightShift = 16.f;
  std::array<float, 4> _heightLevels = {16, 128, 192, 256};
  int _minTessellationLevel = 4, _maxTessellationLevel = 32;
  float _minDistance = 30, _maxDistance = 100;
  bool _enableEdge = false;
  bool _showLoD = false;
  bool _enableLighting = true;
  bool _enableShadow = true;
  std::vector<int> _patchTextures;
  std::vector<int> _patchRotationsIndex;
  std::vector<bool> _changedHeightmap;
  std::vector<bool> _changeMesh, _reallocatePatch, _changePatch;
  std::pair<int, int> _patchNumber;

  int _calculateTileByPosition(glm::vec3 position);
  glm::ivec2 _calculatePixelByPosition(glm::vec3 position);
  void _changeHeightmap(glm::ivec2 position, int value);
  void _calculateMesh(int index);
  int _saveHeightmap(std::string path);
  void _loadHeightmap(std::string path);

 public:
  void setTerrainPhysics(std::shared_ptr<TerrainPhysics> terrainPhysics);
  void setTessellationLevel(int min, int max);
  void setDisplayDistance(int min, int max);
  void setColorHeightLevels(std::array<float, 4> levels);
  void setHeight(float scale, float shift);

  void setMaterial(std::shared_ptr<MaterialColor> material);
  void setDrawType(DrawType drawType);
  void setTileRotation(int tileID, int angle);
  void setTileTexture(int tileID, int textureID);

  DrawType getDrawType();
  void patchEdge(bool enable);
  void showLoD(bool enable);
  bool heightmapChanged();
  std::shared_ptr<ImageCPU<uint8_t>> getHeightmap();

  void transfer(std::shared_ptr<CommandBuffer> commandBuffer);
  void draw(std::shared_ptr<CommandBuffer> commandBuffer) = 0;
  virtual void drawDebug() = 0;

  void cursorNotify(float xPos, float yPos) = 0;
  void mouseNotify(int button, int action, int mods) = 0;
  void keyNotify(int key, int scancode, int action, int mods) = 0;
  void charNotify(unsigned int code) = 0;
  void scrollNotify(double xOffset, double yOffset) = 0;
};

class TerrainGPU : public Drawable, public Shadowable {
 protected:
  std::shared_ptr<EngineState> _engineState;

  std::shared_ptr<MeshStatic3D> _mesh;
  std::shared_ptr<Material> _material;
  std::shared_ptr<Texture> _heightMap;
  std::shared_ptr<ImageCPU<uint8_t>> _heightMapCPU;
  std::shared_ptr<BufferImage> _heightMapGPU;
  MaterialType _materialType = MaterialType::COLOR;
  std::vector<bool> _changedMaterial;
  std::shared_ptr<DescriptorSet> _descriptorSetColor, _descriptorSetPhong, _descriptorSetPBR;

  float _heightScale = 64.f;
  float _heightShift = 16.f;
  std::array<float, 4> _heightLevels = {16, 128, 192, 256};
  int _minTessellationLevel = 4, _maxTessellationLevel = 32;
  float _minDistance = 30, _maxDistance = 100;
  bool _enableLighting = true;
  bool _enableShadow = true;
  std::vector<int> _patchTextures;
  std::vector<int> _patchRotationsIndex;
  std::pair<int, int> _patchNumber = {32, 32};

  void _calculateMesh(std::shared_ptr<CommandBuffer> commandBuffer);

 public:
  virtual void initialize(std::shared_ptr<CommandBuffer> commandBuffer) = 0;
  void setPatchNumber(int x, int y);
  void setPatchRotations(std::vector<int> patchRotationsIndex);
  void setPatchTextures(std::vector<int> patchTextures);
  void setTessellationLevel(int min, int max);
  void setDisplayDistance(int min, int max);
  void setColorHeightLevels(std::array<float, 4> levels);
  void setHeight(float scale, float shift);

  void enableShadow(bool enable);
  void enableLighting(bool enable);
  void setMaterial(std::shared_ptr<MaterialColor> material);
  void setMaterial(std::shared_ptr<MaterialPhong> material);
  void setMaterial(std::shared_ptr<MaterialPBR> material);

  void draw(std::shared_ptr<CommandBuffer> commandBuffer) = 0;
  void drawShadow(LightType lightType, int lightIndex, int face, std::shared_ptr<CommandBuffer> commandBuffer) = 0;
};