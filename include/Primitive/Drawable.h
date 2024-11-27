#pragma once
#include "Utility/Settings.h"
#include "Vulkan/Command.h"
#include "Graphic/LightManager.h"
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
  glm::vec3 _originShift = glm::vec3{0.f, 0.f, 0.f};
  glm::vec3 _translate = glm::vec3{0.f, 0.f, 0.f};
  glm::quat _rotate = glm::identity<glm::quat>();
  glm::vec3 _scale = glm::vec3{1.f, 1.f, 1.f};

 public:
  virtual void draw(std::shared_ptr<CommandBuffer> commandBuffer) = 0;
  void setOriginShift(glm::vec3 originShift);
  void setTranslate(glm::vec3 translate);
  void setRotate(glm::quat rotate);
  void setScale(glm::vec3 scale);
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
