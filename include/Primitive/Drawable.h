#pragma once
#include "Utility/Settings.h"
#include "Vulkan/Command.h"
#include "Graphic/LightManager.h"
#include "Graphic/Camera.h"
#undef OPAQUE
#undef TRANSPARENT

enum class AlphaType { TRANSPARENT, OPAQUE };

class Named {
 private:
  std::string _name;

 public:
  void setName(std::string name);
  std::string getName();
};

class Drawable : virtual public Named {
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
class Shadowable : virtual public Named {
 public:
  virtual void drawShadow(LightType lightType,
                          int lightIndex,
                          int face,
                          std::shared_ptr<CommandBuffer> commandBuffer) = 0;
};
