#include "LightManager.h"
#include "Buffer.h"

LightManager::LightManager(std::shared_ptr<Settings> settings, std::shared_ptr<Device> device) {
  _settings = settings;
  _device = device;
  _descriptorPool = std::make_shared<DescriptorPool>(_descriptorPoolSize, device);

  _descriptorSetLayout = std::make_shared<DescriptorSetLayout>(device);
  _descriptorSetLayout->createLight();
}

std::shared_ptr<PointLight> LightManager::createPointLight() {
  auto light = std::make_shared<PointLight>();
  _pointLights.push_back(light);
  _changed = true;
  return light;
}

std::vector<std::shared_ptr<PointLight>> LightManager::getPointLights() { return _pointLights; }

std::shared_ptr<DirectionalLight> LightManager::createDirectionalLight() {
  auto light = std::make_shared<DirectionalLight>();
  _directionalLights.push_back(light);
  _changed = true;
  return light;
}

std::vector<std::shared_ptr<DirectionalLight>> LightManager::getDirectionalLight() { return _directionalLights; }

std::shared_ptr<DescriptorSetLayout> LightManager::getDescriptorSetLayout() { return _descriptorSetLayout; }

std::shared_ptr<DescriptorSet> LightManager::getDescriptorSet() { return _descriptorSet; }

void LightManager::draw(int frame) {
  if (_changed) {
    int size = 0;
    size += sizeof(int);
    for (int i = 0; i < _directionalLights.size(); i++) {
      size += _directionalLights[i]->getSize();
    }
    _lightDirectionalSSBO = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _device);

    size = 0;
    size += sizeof(int);
    for (int i = 0; i < _pointLights.size(); i++) {
      size += _pointLights[i]->getSize();
    }
    _lightPointSSBO = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _device);

    _descriptorSet = std::make_shared<DescriptorSet>(_settings->getMaxFramesInFlight(), _descriptorSetLayout,
                                                     _descriptorPool, _device);
    _descriptorSet->createLight(_lightDirectionalSSBO, _lightPointSSBO);
    _changed = false;
  }

  _lightDirectionalSSBO->map();
  int offset = 0;
  int directionalLightsNum = _directionalLights.size();
  memcpy((uint8_t*)(_lightDirectionalSSBO->getMappedMemory()) + offset, &directionalLightsNum, sizeof(int));
  // align is 16 bytes, so even for int
  offset += 16;
  for (int i = 0; i < _directionalLights.size(); i++) {
    // we pass inverse bind matrices to shader via SSBO
    memcpy((uint8_t*)(_lightDirectionalSSBO->getMappedMemory()) + offset, _directionalLights[i]->getData(),
           _directionalLights[i]->getSize());
    offset += _directionalLights[i]->getSize();
  }

  _lightDirectionalSSBO->unmap();

  _lightPointSSBO->map();
  offset = 0;
  int pointLightsNum = _pointLights.size();
  memcpy((uint8_t*)(_lightPointSSBO->getMappedMemory()) + offset, &pointLightsNum, sizeof(int));
  offset += 16;
  for (int i = 0; i < _pointLights.size(); i++) {
    // we pass inverse bind matrices to shader via SSBO
    memcpy((uint8_t*)(_lightPointSSBO->getMappedMemory()) + offset, _pointLights[i]->getData(),
           _pointLights[i]->getSize());
    offset += _pointLights[i]->getSize();
  }
  _lightPointSSBO->unmap();
}