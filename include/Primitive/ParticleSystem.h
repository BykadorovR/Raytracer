#pragma once
#include "State.h"
#include "Camera.h"

struct Particle {
  alignas(16) glm::vec3 position;
  alignas(16) glm::vec3 velocity;
  alignas(16) glm::vec4 color;

  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Particle);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Particle, position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Particle, color);

    return attributeDescriptions;
  }
};

class ParticleSystem {
 private:
  int _particlesNumber;
  std::shared_ptr<State> _state;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;
  glm::mat4 _model = glm::mat4(1.f);
  std::shared_ptr<Camera> _camera;
  std::shared_ptr<Texture> _texture;
  std::shared_ptr<UniformBuffer> _deltaUniformBuffer, _cameraUniformBuffer;
  std::vector<std::shared_ptr<Buffer>> _particlesBuffer;
  std::vector<std::shared_ptr<DescriptorSet>> _descriptorSetCompute;
  std::shared_ptr<DescriptorSet> _descriptorSetCamera, _descriptorSetTexture;
  std::shared_ptr<Pipeline> _computePipeline, _graphicPipeline;
  float _frameTimer = 0.f;

  void _initializeCompute();
  void _initializeGraphic();

 public:
  ParticleSystem(int particlesNumber,
                 std::shared_ptr<Texture> texture,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<State> state);
  void setModel(glm::mat4 model);
  void setCamera(std::shared_ptr<Camera> camera);
  void updateTimer(float frameTimer);

  void drawCompute(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer);
  void drawGraphic(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer);
};