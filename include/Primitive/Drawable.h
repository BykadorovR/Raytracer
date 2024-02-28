#pragma once
#include "Settings.h"
#include "Command.h"
#include "LightManager.h"
#include "Camera.h"
#undef OPAQUE
#undef TRANSPARENT

enum class AlphaType { TRANSPARENT, OPAQUE };

class Drawable {
 protected:
  glm::mat4 _model = glm::mat4(1.f);

 public:
  virtual void draw(std::tuple<int, int> resolution,
                    std::shared_ptr<Camera> camera,
                    std::shared_ptr<CommandBuffer> commandBuffer) = 0;
  void setModel(glm::mat4 model);
  glm::mat4 getModel();
};

// Such objects can be influenced by shadow (shadows can appear on them and they can generate shadows)
class Shadowable {
 public:
  virtual void drawShadow(LightType lightType,
                          int lightIndex,
                          int face,
                          std::shared_ptr<CommandBuffer> commandBuffer) = 0;
};
