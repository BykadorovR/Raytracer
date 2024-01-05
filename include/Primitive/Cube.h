#pragma once
#include "Drawable.h"
#include "State.h"
#include "Cubemap.h"
#include "Camera.h"
#include "Material.h"
#include "Mesh.h"
#include "LightManager.h"
#include "Shape3D.h"

class Cube : public IDrawable, IShadowable {
 private:
  std::shared_ptr<Mesh3D> _mesh;
  std::shared_ptr<Shape3D> _shape3D;

 public:
  Cube(std::vector<VkFormat> renderFormat,
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
            DrawType drawType = DrawType::FILL) override;
  void drawShadow(int currentFrame,
                  std::shared_ptr<CommandBuffer> commandBuffer,
                  LightType lightType,
                  int lightIndex,
                  int face = 0) override;
};