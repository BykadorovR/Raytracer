#pragma once
#include "Light.h"
#include "Camera.h"
#include "Command.h"
#include "Pipeline.h"
#include "Descriptor.h"
#include <vector>
#include <memory>

class LightManager {
 private:
  int _descriptorPoolSize = 100;
  std::vector<std::shared_ptr<Light>> _lights;
  std::shared_ptr<Device> _device;
  std::shared_ptr<Settings> _settings;
  std::shared_ptr<DescriptorPool> _descriptorPool;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayout;
  std::shared_ptr<Buffer> _lightsSSBO;
  std::shared_ptr<DescriptorSet> _descriptorSet;
  bool _changed = false;

 public:
  LightManager(std::shared_ptr<Settings> settings, std::shared_ptr<Device> device);
  std::shared_ptr<PhongLight> createPhongLight(glm::vec3 position, glm::vec3 color, float ambient, float specular);
  std::vector<std::shared_ptr<Light>> getLights();
  std::shared_ptr<DescriptorSetLayout> getDescriptorSetLayout();
  std::shared_ptr<DescriptorSet> getDescriptorSet();
  void draw(int frame);
};