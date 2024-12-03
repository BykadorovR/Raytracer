#pragma once
#include <memory>
#include <vector>
#include "Graphic/Texture.h"
#include "Primitive/Cubemap.h"
#include "Utility/Settings.h"
#include "Graphic/Camera.h"
#include "Utility/Logger.h"
#include "Vulkan/Debug.h"
#include "Vulkan/Render.h"

class DirectionalShadow {
 protected:
  std::shared_ptr<EngineState> _engineState;
  std::shared_ptr<CommandBuffer> _commandBufferDirectional;
  std::shared_ptr<Logger> _loggerDirectional;
  std::vector<std::shared_ptr<Texture>> _shadowMapTexture;
  std::vector<std::shared_ptr<Framebuffer>> _shadowMapFramebuffer;

 public:
  DirectionalShadow(std::shared_ptr<CommandBuffer> commandBufferTransfer,
                    std::shared_ptr<RenderPass> renderPass,
                    std::shared_ptr<DebuggerUtils> debuggerUtils,
                    std::shared_ptr<EngineState> engineState);
  std::vector<std::shared_ptr<Texture>> getShadowMapTexture();

  std::shared_ptr<CommandBuffer> getShadowMapCommandBuffer();
  std::shared_ptr<Logger> getShadowMapLogger();
  std::vector<std::shared_ptr<Framebuffer>> getShadowMapFramebuffer();
};

class PointShadow {
 protected:
  std::shared_ptr<EngineState> _engineState;
  std::vector<std::shared_ptr<CommandBuffer>> _commandBufferPoint;
  std::vector<std::shared_ptr<Logger>> _loggerPoint;
  std::vector<std::shared_ptr<Cubemap>> _shadowMapCubemap;
  std::vector<std::vector<std::shared_ptr<Framebuffer>>> _shadowMapFramebuffer;

 public:
  PointShadow(std::shared_ptr<CommandBuffer> commandBufferTransfer,
              std::shared_ptr<RenderPass> renderPass,
              std::shared_ptr<DebuggerUtils> debuggerUtils,
              std::shared_ptr<EngineState> engineState);
  std::vector<std::shared_ptr<Cubemap>> getShadowMapCubemap();
  std::vector<std::shared_ptr<CommandBuffer>> getShadowMapCommandBuffer();
  std::vector<std::shared_ptr<Logger>> getShadowMapLogger();
  std::vector<std::vector<std::shared_ptr<Framebuffer>>> getShadowMapFramebuffer();
};
