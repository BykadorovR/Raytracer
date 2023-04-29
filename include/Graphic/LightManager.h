#pragma once
#include "Light.h"
#include "Command.h"
#include "Pipeline.h"
#include "Descriptor.h"
#include "Settings.h"
#include <vector>
#include <memory>

class LightManager {
 private:
  int _descriptorPoolSize = 100;
  std::vector<std::shared_ptr<PointLight>> _pointLights;
  std::vector<std::shared_ptr<DirectionalLight>> _directionalLights;

  std::shared_ptr<Device> _device;
  std::shared_ptr<Settings> _settings;
  std::shared_ptr<DescriptorPool> _descriptorPool;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayout;
  std::shared_ptr<Buffer> _lightDirectionalSSBO = nullptr, _lightPointSSBO = nullptr;
  std::shared_ptr<DescriptorSet> _descriptorSet;
  bool _changed = true;

 public:
  LightManager(std::shared_ptr<Settings> settings, std::shared_ptr<Device> device);
  std::shared_ptr<PointLight> createPointLight();
  std::vector<std::shared_ptr<PointLight>> getPointLights();
  std::shared_ptr<DirectionalLight> createDirectionalLight();
  std::vector<std::shared_ptr<DirectionalLight>> getDirectionalLight();
  std::shared_ptr<DescriptorSetLayout> getDescriptorSetLayout();
  std::shared_ptr<DescriptorSet> getDescriptorSet();
  void draw(int frame);
};