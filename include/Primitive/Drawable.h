#pragma once
#include "Settings.h"
#include "Command.h"
#include "LightManager.h"

class IDrawable {
 public:
  virtual void draw(std::tuple<int, int> resolution, std::shared_ptr<CommandBuffer> commandBuffer) = 0;
};

// Such objects can be influenced by shadow (shadows can appear on them and they can generate shadows)
class IShadowable {
 public:
  virtual void drawShadow(std::shared_ptr<CommandBuffer> commandBuffer,
                          LightType lightType,
                          int lightIndex,
                          int face = 0) = 0;
};