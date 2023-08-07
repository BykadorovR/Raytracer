#pragma once
#include "ModelManager.h"
#include "GUI.h"
#include "LightManager.h"
#include "State.h"
#include "Sphere.h"
#include "Line.h"
#include "Sprite.h"
#include "SpriteManager.h"
#undef near
#undef far

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
  std::vector<std::shared_ptr<ModelGLTF>> _pointLightModels, _directionalLightModels;
  std::vector<std::shared_ptr<Sphere>> _spheres;
  std::shared_ptr<LightManager> _lightManager = nullptr;
  bool _showLights = true;
  bool _registerLights = false;
  std::shared_ptr<Camera> _camera;
  std::shared_ptr<Model3DManager> _modelManager;
  std::shared_ptr<SpriteManager> _spriteManager;
  std::shared_ptr<GUI> _gui;
  std::shared_ptr<State> _state;
  bool _cursorEnabled = false;
  std::shared_ptr<Pipeline> _pipeline;
  std::shared_ptr<UniformBuffer> _uniformBuffer;
  std::shared_ptr<DescriptorSetLayout> _textureSetLayout;
  std::vector<std::shared_ptr<DescriptorSet>> _descriptorSetDirectional;
  std::vector<std::vector<std::shared_ptr<DescriptorSet>>> _descriptorSetPoint;
  std::shared_ptr<DescriptorSet> _cameraSet;
  std::shared_ptr<VertexBuffer<Vertex2D>> _vertexBuffer;
  std::shared_ptr<VertexBuffer<uint32_t>> _indexBuffer;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;
  bool _showDepth = true;
  bool _initializedDepth = false;
  int _lightSpheresIndex = -1;
  int _shadowMapIndex = 0;
  bool _enableSpheres = false;
  std::vector<std::string> _attenuationKeys;
  std::vector<std::string> _shadowKeys;

  bool _frustumDraw = false;
  bool _showPlanes = false;
  bool _planesRegistered = false;
  float _near, _far;
  std::vector<std::shared_ptr<Line>> _lineFrustum;
  std::shared_ptr<Sprite> _farPlaneCW, _farPlaneCCW;
  glm::vec3 _eyeSave = {0, 0, 3}, _dirSave = {0, 0, -1}, _upSave = {0, 1, 0}, _angles = {-90, 0, 0};

  std::vector<Vertex2D> _vertices = {
      {{0.5f, 0.5f, 0.f}, {0.f, 0.f, -1.f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.f, 0.f}},
      {{0.5f, -0.5f, 0.f}, {0.f, 0.f, -1.f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.f, 0.f}},
      {{-0.5f, -0.5f, 0.f}, {0.f, 0.f, -1.f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.f, 0.f}},
      {{-0.5f, 0.5f, 0.f}, {0.f, 0.f, -1.f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.f, 0.f}}};

  const std::vector<uint32_t> _indices = {0, 1, 3, 1, 2, 3};

  void _drawFrustum(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer);
  void _drawShadowMaps(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer);

 public:
  DebugVisualization(std::shared_ptr<Camera> camera,
                     std::shared_ptr<GUI> gui,
                     std::shared_ptr<CommandBuffer> commandBufferTransfer,
                     std::shared_ptr<State> state);
  void setSpriteManager(std::shared_ptr<SpriteManager> spriteManager);
  void setLights(std::shared_ptr<Model3DManager> modelManager, std::shared_ptr<LightManager> lightManager);
  void draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer);

  void cursorNotify(GLFWwindow* window, float xPos, float yPos) override;
  void mouseNotify(GLFWwindow* window, int button, int action, int mods) override;
  void keyNotify(GLFWwindow* window, int key, int scancode, int action, int mods) override;
  void charNotify(GLFWwindow* window, unsigned int code) override;
  void scrollNotify(GLFWwindow* window, double xOffset, double yOffset) override;
};