#pragma once
#include "ModelManager.h"
#include "GUI.h"
#include "LightManager.h"
#include "State.h"
#include "Logger.h"

struct DepthPush {
  float near;
  float far;
  static VkPushConstantRange getPushConstant() {
    VkPushConstantRange pushConstant;
    // this push constant range starts at the beginning
    pushConstant.offset = 0;
    // this push constant range takes up the size of a MeshPushConstants struct
    pushConstant.size = sizeof(DepthPush);
    // this push constant range is accessible only in the vertex shader
    pushConstant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    return pushConstant;
  }
};

class DebugVisualization : public InputSubscriber {
 private:
  std::vector<std::shared_ptr<ModelGLTF>> _lightModels;
  std::shared_ptr<LightManager> _lightManager = nullptr;
  bool _showLights = true;
  bool _registerLights = false;
  std::shared_ptr<Camera> _camera;
  std::shared_ptr<Model3DManager> _modelManager;
  std::shared_ptr<GUI> _gui;
  std::shared_ptr<State> _state;
  std::shared_ptr<Texture> _texture = nullptr;
  bool _cursorEnabled = false;
  std::shared_ptr<RenderPass> _renderPass;
  std::shared_ptr<Pipeline> _pipeline;
  std::shared_ptr<UniformBuffer> _uniformBuffer;
  std::shared_ptr<DescriptorSetLayout> _textureSetLayout;
  std::shared_ptr<DescriptorSet> _textureSet;
  std::shared_ptr<DescriptorSet> _cameraSet;
  std::shared_ptr<VertexBuffer2D> _vertexBuffer;
  std::shared_ptr<IndexBuffer> _indexBuffer;
  std::shared_ptr<Logger> _logger;

  std::vector<Vertex2D> _vertices = {
      {{0.5f, 0.5f, 0.f}, {0.f, 0.f, -1.f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.f, 0.f}},
      {{0.5f, -0.5f, 0.f}, {0.f, 0.f, -1.f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.f, 0.f}},
      {{-0.5f, -0.5f, 0.f}, {0.f, 0.f, -1.f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.f, 0.f}},
      {{-0.5f, 0.5f, 0.f}, {0.f, 0.f, -1.f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.f, 0.f}}};

  const std::vector<uint32_t> _indices = {0, 1, 3, 1, 2, 3};

 public:
  DebugVisualization(std::shared_ptr<Camera> camera,
                     std::shared_ptr<GUI> gui,
                     std::shared_ptr<State> state,
                     std::shared_ptr<Logger> logger);
  void setLights(std::shared_ptr<Model3DManager> modelManager, std::shared_ptr<LightManager> lightManager);
  void setTexture(std::shared_ptr<Texture> texture);
  void draw(int currentFrame);

  void cursorNotify(GLFWwindow* window, float xPos, float yPos);
  void mouseNotify(GLFWwindow* window, int button, int action, int mods);
  void keyNotify(GLFWwindow* window, int key, int action, int mods);
};