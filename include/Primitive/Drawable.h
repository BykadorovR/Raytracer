#pragma once
#include "Settings.h"
#include "Command.h"
#include "LightManager.h"

class Drawable {
 public:
  virtual void draw(int currentFrame,
                    std::tuple<int, int> resolution,
                    std::shared_ptr<CommandBuffer> commandBuffer,
                    DrawType drawType = DrawType::FILL) = 0;
  virtual void drawShadow(int currentFrame,
                          std::shared_ptr<CommandBuffer> commandBuffer,
                          LightType lightType,
                          int lightIndex,
                          int face = 0) = 0;
};