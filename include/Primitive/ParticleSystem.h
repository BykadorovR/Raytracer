#pragma once
#include "Utility/EngineState.h"
#include "Utility/GameState.h"
#include "Graphic/Camera.h"
#include "Graphic/Texture.h"
#include "Vulkan/Descriptor.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/Render.h"
#include "Primitive/Drawable.h"

struct Particle {
  glm::vec3 position;
  float radius;
  glm::vec4 color;
  glm::vec4 minColor;
  glm::vec4 maxColor;
  float life;
  float minLife;
  float maxLife;
  int iteration;
  float velocity;
  float minVelocity;
  float maxVelocity;
  alignas(16) glm::vec3 velocityDirection;
};

class ParticleSystem : public Drawable {
 private:
  std::shared_ptr<EngineState> _engineState;
  std::shared_ptr<GameState> _gameState;

  std::vector<Particle> _particles;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;
  std::shared_ptr<Texture> _texture;
  std::vector<std::shared_ptr<Buffer>> _deltaUniformBuffer, _cameraUniformBuffer;
  std::vector<std::shared_ptr<Buffer>> _particlesBuffer;
  std::shared_ptr<DescriptorSet> _descriptorSetCompute;
  std::shared_ptr<DescriptorSet> _descriptorSetGraphic;
  std::shared_ptr<PipelineGraphic> _graphicPipeline;
  std::shared_ptr<PipelineCompute> _computePipeline;
  float _frameTimer = 0.f;
  float _pointScale = 60.f;

  void _initializeCompute();
  void _initializeGraphic();

 public:
  ParticleSystem(std::vector<Particle> particles,
                 std::shared_ptr<Texture> texture,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<GameState> gameState,
                 std::shared_ptr<EngineState> engineState);
  void setPointScale(float pointScale);
  void updateTimer(float frameTimer);

  void drawCompute(std::shared_ptr<CommandBuffer> commandBuffer);
  void draw(std::shared_ptr<CommandBuffer> commandBuffer) override;
};