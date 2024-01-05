#pragma once

#include "State.h"
#include "Camera.h"
#include "Mesh.h"
#include "Descriptor.h"
#include "Pipeline.h"
#include "Drawable.h"
#include "Material.h"

enum class ShapeType { CUBE = 0, SPHERE = 1 };

class Shape3D {
 private:
  std::map<ShapeType, std::pair<std::string, std::string>> _shapeShaders = {
      {ShapeType::CUBE, {"shaders/cube_vertex.spv", "shaders/cube_fragment.spv"}},
      {ShapeType::SPHERE, {"shaders/sphere_vertex.spv", "shaders/sphere_fragment.spv"}}};
  std::map<ShapeType, std::pair<std::string, std::string>> _shapeShadersDepth = {
      {ShapeType::CUBE, {"shaders/depthCube_vertex.spv", "shaders/depthCube_fragment.spv"}},
      {ShapeType::SPHERE, {"shaders/sphereDepth_vertex.spv", "shaders/sphereDepth_fragment.spv"}}};
  ShapeType _shapeType;
  std::shared_ptr<State> _state;
  std::shared_ptr<Mesh3D> _mesh;
  std::shared_ptr<UniformBuffer> _uniformBuffer;
  std::shared_ptr<DescriptorSet> _descriptorSetCamera;
  std::vector<std::vector<std::shared_ptr<UniformBuffer>>> _cameraUBODepth;
  std::vector<std::vector<std::shared_ptr<DescriptorSet>>> _descriptorSetCameraDepth;
  std::shared_ptr<Pipeline> _pipeline, _pipelineWireframe, _pipelineDirectional, _pipelinePoint;
  std::shared_ptr<Camera> _camera;
  std::shared_ptr<Material> _material;
  std::shared_ptr<MaterialColor> _defaultMaterialColor;
  std::shared_ptr<LightManager> _lightManager;
  MaterialType _materialType = MaterialType::COLOR;
  glm::mat4 _model = glm::mat4(1.f);

 public:
  Shape3D(ShapeType shapeType,
          std::shared_ptr<Mesh3D> mesh,
          std::vector<VkFormat> renderFormat,
          VkCullModeFlags cullMode,
          std::shared_ptr<LightManager> lightManager,
          std::shared_ptr<CommandBuffer> commandBufferTransfer,
          std::shared_ptr<State> state);

  void setMaterial(std::shared_ptr<MaterialColor> material);
  void setModel(glm::mat4 model);
  void setCamera(std::shared_ptr<Camera> camera);
  std::shared_ptr<Mesh3D> getMesh();

  void draw(int currentFrame,
            std::tuple<int, int> resolution,
            std::shared_ptr<CommandBuffer> commandBuffer,
            DrawType drawType = DrawType::FILL);
  void drawShadow(int currentFrame,
                  std::shared_ptr<CommandBuffer> commandBuffer,
                  LightType lightType,
                  int lightIndex,
                  int face = 0);
};