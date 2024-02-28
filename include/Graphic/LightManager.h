#pragma once
#include "Light.h"
#include "Command.h"
#include "Pipeline.h"
#include "Descriptor.h"
#include "Settings.h"
#include <vector>
#include <memory>
#include "State.h"
#include "Logger.h"
#include "Material.h"
#include "ResourceManager.h"

enum class LightType { DIRECTIONAL = 0, POINT = 1, AMBIENT = 2 };

class LightManager {
 private:
  int _descriptorPoolSize = 100;

  std::vector<std::shared_ptr<PointLight>> _pointLights;
  std::vector<std::shared_ptr<DirectionalLight>> _directionalLights;
  std::vector<std::shared_ptr<AmbientLight>> _ambientLights;

  std::shared_ptr<State> _state;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;
  std::shared_ptr<DescriptorPool> _descriptorPool;
  std::vector<std::shared_ptr<Buffer>> _lightDirectionalSSBO, _lightPointSSBO, _lightAmbientSSBO;
  std::vector<std::shared_ptr<Buffer>> _lightDirectionalSSBOViewProjection, _lightPointSSBOViewProjection;
  std::shared_ptr<Texture> _stubTexture;
  std::shared_ptr<Cubemap> _stubCubemap;
  std::shared_ptr<DescriptorSet> _descriptorSetLightPhong, _descriptorSetLightPBR;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayoutLightPhong, _descriptorSetLayoutLightPBR;
  std::map<VkShaderStageFlagBits, std::shared_ptr<DescriptorSet>> _descriptorSetViewProjection;
  std::map<VkShaderStageFlagBits, std::shared_ptr<DescriptorSetLayout>> _descriptorSetLayoutViewProjection;
  std::vector<std::shared_ptr<CommandBuffer>> _commandBufferDirectional;
  std::vector<std::vector<std::shared_ptr<CommandBuffer>>> _commandBufferPoint;
  std::vector<std::shared_ptr<LoggerGPU>> _loggerGPUDirectional;
  std::vector<std::vector<std::shared_ptr<LoggerGPU>>> _loggerGPUPoint;
  // for 2 frames
  std::vector<std::shared_ptr<DescriptorSet>> _descriptorSetDepthTexture;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayoutDepthTexture;
  std::mutex _accessMutex;

  std::map<LightType, std::vector<bool>> _changed;
  std::vector<std::shared_ptr<Texture>> _directionalTextures;
  std::vector<std::shared_ptr<Texture>> _pointTextures;
  void _reallocateDirectionalDescriptors(int currentFrame);
  void _updateDirectionalDescriptors(int currentFrame);
  void _updateDirectionalTexture(int currentFrame);
  void _reallocatePointDescriptors(int currentFrame);
  void _updatePointDescriptors(int currentFrame);
  void _updatePointTexture(int currentFrame);
  void _reallocateAmbientDescriptors(int currentFrame);
  void _updateAmbientDescriptors(int currentFrame);
  void _setLightDescriptors(int currentFrame);

 public:
  LightManager(std::shared_ptr<CommandBuffer> commandBufferTransfer,
               std::shared_ptr<ResourceManager> resourceManager,
               std::shared_ptr<State> state);

  // Lights can't be added AFTER draw for current frame, only before draw.
  std::shared_ptr<AmbientLight> createAmbientLight();
  std::vector<std::shared_ptr<AmbientLight>> getAmbientLights();
  std::shared_ptr<PointLight> createPointLight(std::tuple<int, int> resolution);
  const std::vector<std::shared_ptr<PointLight>>& getPointLights();
  void removePointLights(std::shared_ptr<PointLight> pointLight);
  const std::vector<std::vector<std::shared_ptr<CommandBuffer>>>& getPointLightCommandBuffers();
  const std::vector<std::vector<std::shared_ptr<LoggerGPU>>>& getPointLightLoggers();

  std::shared_ptr<DirectionalLight> createDirectionalLight(std::tuple<int, int> resolution);
  const std::vector<std::shared_ptr<DirectionalLight>>& getDirectionalLights();
  void removeDirectionalLight(std::shared_ptr<DirectionalLight> directionalLight);
  const std::vector<std::shared_ptr<CommandBuffer>>& getDirectionalLightCommandBuffers();
  const std::vector<std::shared_ptr<LoggerGPU>>& getDirectionalLightLoggers();

  std::shared_ptr<DescriptorSetLayout> getDSLLightPhong();
  std::shared_ptr<DescriptorSetLayout> getDSLLightPBR();
  std::shared_ptr<DescriptorSet> getDSLightPhong();
  std::shared_ptr<DescriptorSet> getDSLightPBR();
  std::shared_ptr<DescriptorSetLayout> getDSLViewProjection(VkShaderStageFlagBits stage);
  std::shared_ptr<DescriptorSet> getDSViewProjection(VkShaderStageFlagBits stage);
  std::shared_ptr<DescriptorSetLayout> getDSLShadowTexture();
  std::vector<std::shared_ptr<DescriptorSet>> getDSShadowTexture();
  void draw(int currentFrame);
};