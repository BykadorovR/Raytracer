#include "LightManager.h"
#include "Buffer.h"

LightManager::LightManager(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state) {
  _state = state;
  _commandBufferTransfer = commandBufferTransfer;

  _descriptorPool = std::make_shared<DescriptorPool>(_descriptorPoolSize, _state->getDevice());

  _descriptorSetLayoutLight = std::make_shared<DescriptorSetLayout>(_state->getDevice());
  _descriptorSetLayoutLight->createLight();

  _descriptorSetLayoutViewProjection = std::make_shared<DescriptorSetLayout>(_state->getDevice());
  _descriptorSetLayoutViewProjection->createLightVP();

  _descriptorSetLayoutDepthTexture = std::make_shared<DescriptorSetLayout>(_state->getDevice());
  _descriptorSetLayoutDepthTexture->createShadowTexture();

  // stub texture
  _stubTexture = std::make_shared<Texture>("../data/Texture1x1.png", VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                           commandBufferTransfer, _state->getDevice());
  _stubCubemap = std::make_shared<Cubemap>("../data/Texture1x1.png", commandBufferTransfer, _state);
}

std::shared_ptr<PointLight> LightManager::createPointLight(std::tuple<int, int> resolution) {
  std::vector<std::shared_ptr<Cubemap>> depthCubemap;
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    depthCubemap.push_back(std::make_shared<Cubemap>(resolution, _commandBufferTransfer, _state));
  }

  auto light = std::make_shared<PointLight>(_state->getSettings());
  light->setDepthCubemap(depthCubemap);
  _pointLights.push_back(light);
  _changed = true;
  return light;
}

std::vector<std::shared_ptr<PointLight>> LightManager::getPointLights() { return _pointLights; }

std::shared_ptr<DirectionalLight> LightManager::createDirectionalLight(std::tuple<int, int> resolution) {
  std::vector<std::shared_ptr<Texture>> depthTexture;
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    std::shared_ptr<Image> image = std::make_shared<Image>(
        resolution, 1, _state->getSettings()->getDepthFormat(), VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        _state->getDevice());
    image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                        VK_IMAGE_ASPECT_DEPTH_BIT, 1, _commandBufferTransfer);
    auto imageView = std::make_shared<ImageView>(image, VK_IMAGE_VIEW_TYPE_2D, 1, 0, VK_IMAGE_ASPECT_DEPTH_BIT,
                                                 _state->getDevice());
    depthTexture.push_back(
        std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, imageView, _state->getDevice()));
  }

  auto light = std::make_shared<DirectionalLight>();
  light->setDepthTexture(depthTexture);
  _directionalLights.push_back(light);
  _changed = true;
  return light;
}

std::vector<std::shared_ptr<DirectionalLight>> LightManager::getDirectionalLights() { return _directionalLights; }

std::shared_ptr<DescriptorSetLayout> LightManager::getDSLViewProjection() { return _descriptorSetLayoutViewProjection; }

std::shared_ptr<DescriptorSet> LightManager::getDSViewProjection() { return _descriptorSetViewProjection; }

std::shared_ptr<DescriptorSetLayout> LightManager::getDSLLight() { return _descriptorSetLayoutLight; }

std::shared_ptr<DescriptorSet> LightManager::getDSLight() { return _descriptorSetLight; }

std::shared_ptr<DescriptorSetLayout> LightManager::getDSLShadowTexture() { return _descriptorSetLayoutDepthTexture; }

std::vector<std::shared_ptr<DescriptorSet>> LightManager::getDSShadowTexture() { return _descriptorSetDepthTexture; }

void LightManager::draw(int frame) {
  if (_changed) {
    int size = 0;
    // align is 16 bytes, so even for int because in our SSBO struct
    // we have fields 16 bytes size so the whole struct has 16 bytes allignment
    size += sizeof(glm::vec4);
    for (int i = 0; i < _directionalLights.size(); i++) {
      size += _directionalLights[i]->getSize();
    }
    _lightDirectionalSSBO = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());

    size = 0;
    size += sizeof(glm::vec4);
    for (int i = 0; i < _pointLights.size(); i++) {
      size += _pointLights[i]->getSize();
    }
    _lightPointSSBO = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());

    size = 0;
    size += sizeof(glm::vec4);
    for (int i = 0; i < _directionalLights.size(); i++) {
      size += sizeof(glm::mat4);
    }
    _lightDirectionalSSBOViewProjection = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());

    size = 0;
    size += sizeof(glm::vec4);
    for (int i = 0; i < _pointLights.size(); i++) {
      size += sizeof(glm::mat4);
    }
    _lightPointSSBOViewProjection = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());

    _descriptorSetLight = std::make_shared<DescriptorSet>(
        _state->getSettings()->getMaxFramesInFlight(), _descriptorSetLayoutLight, _descriptorPool, _state->getDevice());
    _descriptorSetLight->createLight(_lightDirectionalSSBO, _lightPointSSBO);

    _descriptorSetViewProjection = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                                   _descriptorSetLayoutViewProjection, _descriptorPool,
                                                                   _state->getDevice());
    _descriptorSetViewProjection->createLight(_lightDirectionalSSBOViewProjection, _lightPointSSBOViewProjection);

    _descriptorSetDepthTexture.resize(_state->getSettings()->getMaxFramesInFlight());
    for (int j = 0; j < _state->getSettings()->getMaxFramesInFlight(); j++) {
      std::vector<std::shared_ptr<Texture>> directionalTextures(_state->getSettings()->getMaxDirectionalLights());
      for (int i = 0; i < _state->getSettings()->getMaxDirectionalLights(); i++) {
        if (i < _directionalLights.size())
          directionalTextures[i] = _directionalLights[i]->getDepthTexture()[j];
        else
          directionalTextures[i] = _stubTexture;
      }

      std::vector<std::shared_ptr<Texture>> pointTextures(_state->getSettings()->getMaxPointLights());
      for (int i = 0; i < _state->getSettings()->getMaxPointLights(); i++) {
        if (i < _pointLights.size())
          pointTextures[i] = _pointLights[i]->getDepthCubemap()[j]->getTexture();
        else
          pointTextures[i] = _stubCubemap->getTexture();
      }

      _descriptorSetDepthTexture[j] = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                                      _descriptorSetLayoutDepthTexture, _descriptorPool,
                                                                      _state->getDevice());
      _descriptorSetDepthTexture[j]->createShadowTexture(directionalTextures, pointTextures);
    }
    _changed = false;
  }

  _lightDirectionalSSBO->map();
  int offset = 0;
  int directionalLightsNum = _directionalLights.size();
  memcpy((uint8_t*)(_lightDirectionalSSBO->getMappedMemory()) + offset, &directionalLightsNum, sizeof(int));
  // align is 16 bytes, so even for int
  offset += sizeof(glm::vec4);
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
  offset += sizeof(glm::vec4);
  for (int i = 0; i < _pointLights.size(); i++) {
    // we pass inverse bind matrices to shader via SSBO
    memcpy((uint8_t*)(_lightPointSSBO->getMappedMemory()) + offset, _pointLights[i]->getData(),
           _pointLights[i]->getSize());
    offset += _pointLights[i]->getSize();
  }
  _lightPointSSBO->unmap();

  _lightDirectionalSSBOViewProjection->map();
  offset = 0;
  memcpy((uint8_t*)(_lightDirectionalSSBOViewProjection->getMappedMemory()) + offset, &directionalLightsNum,
         sizeof(int));
  // align is 16 bytes, so even for int
  offset += sizeof(glm::vec4);
  std::vector<glm::mat4> directionalVP;
  for (int i = 0; i < _directionalLights.size(); i++) {
    glm::mat4 viewProjection = _directionalLights[i]->getProjectionMatrix() * _directionalLights[i]->getViewMatrix();
    directionalVP.push_back(viewProjection);
  }
  int test = directionalVP.size() * sizeof(glm::mat4);
  memcpy((uint8_t*)(_lightDirectionalSSBOViewProjection->getMappedMemory()) + offset, directionalVP.data(),
         directionalVP.size() * sizeof(glm::mat4));
  _lightDirectionalSSBOViewProjection->unmap();

  _lightPointSSBOViewProjection->map();
  offset = 0;
  memcpy((uint8_t*)(_lightPointSSBOViewProjection->getMappedMemory()) + offset, &pointLightsNum, sizeof(int));
  // align is 16 bytes, so even for int
  offset += sizeof(glm::vec4);
  std::vector<glm::mat4> pointVP;
  float aspect = (float)std::get<0>(_state->getSettings()->getResolution()) /
                 (float)std::get<1>(_state->getSettings()->getResolution());
  for (int i = 0; i < _pointLights.size(); i++) {
    glm::mat4 viewProjection = _pointLights[i]->getProjectionMatrix() * _pointLights[i]->getViewMatrix(0);
    pointVP.push_back(viewProjection);
  }
  memcpy((uint8_t*)(_lightPointSSBOViewProjection->getMappedMemory()) + offset, pointVP.data(),
         pointVP.size() * sizeof(glm::mat4));
  _lightPointSSBOViewProjection->unmap();
}