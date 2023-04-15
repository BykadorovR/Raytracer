#include "LightManager.h"
#include "Buffer.h"

LightManager::LightManager(std::shared_ptr<Settings> settings, std::shared_ptr<Device> device) {
  _settings = settings;
  _device = device;
  _descriptorPool = std::make_shared<DescriptorPool>(_descriptorPoolSize, device);

  _descriptorSetLayout = std::make_shared<DescriptorSetLayout>(device);
  _descriptorSetLayout->createLight();
}

std::shared_ptr<PhongLight> LightManager::createPhongLight(glm::vec3 position,
                                                           glm::vec3 color,
                                                           float ambient,
                                                           float specular) {
  auto light = std::make_shared<PhongLight>();
  light->setPosition(position);
  light->setColor(color);
  light->setAmbient(ambient);
  light->setSpecular(specular);
  _lights.push_back(light);
  _changed = true;
  return light;
}

std::shared_ptr<DescriptorSetLayout> LightManager::getDescriptorSetLayout() { return _descriptorSetLayout; }

std::shared_ptr<DescriptorSet> LightManager::getDescriptorSet() { return _descriptorSet; }

std::vector<std::shared_ptr<Light>> LightManager::getLights() { return _lights; }

void LightManager::draw(int frame) {
  if (_lights.size() == 0) return;
  if (_changed) {
    int size = 0;
    for (int i = 0; i < _lights.size(); i++) {
      size += _lights[i]->getSize();
    }
    _lightsSSBO = std::make_shared<Buffer>(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                           _device);
    _descriptorSet = std::make_shared<DescriptorSet>(_settings->getMaxFramesInFlight(), _descriptorSetLayout,
                                                     _descriptorPool, _device);
    _descriptorSet->createLight(_lightsSSBO);
    _changed = false;
  }

  _lightsSSBO->map();
  int offset = 0;
  for (int i = 0; i < _lights.size(); i++) {
    // we pass inverse bind matrices to shader via SSBO
    memcpy((uint8_t*)(_lightsSSBO->getMappedMemory()) + offset, _lights[i]->getData(), _lights[i]->getSize());
    offset += _lights[i]->getSize();
  }
  _lightsSSBO->unmap();

  for (auto light : _lights) {
    light->draw();
  }
}