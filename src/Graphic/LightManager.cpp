#include "LightManager.h"
#include "Buffer.h"

LightManager::LightManager(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state) {
  _state = state;
  _commandBufferTransfer = commandBufferTransfer;

  _descriptorPool = std::make_shared<DescriptorPool>(_descriptorPoolSize, _state->getDevice());

  _descriptorSetLayoutLight = std::make_shared<DescriptorSetLayout>(_state->getDevice());
  _descriptorSetLayoutLight->createLight();

  _descriptorSetLayoutViewProjection[VK_SHADER_STAGE_VERTEX_BIT] = std::make_shared<DescriptorSetLayout>(
      _state->getDevice());
  _descriptorSetLayoutViewProjection[VK_SHADER_STAGE_VERTEX_BIT]->createLightVP(VK_SHADER_STAGE_VERTEX_BIT);

  _descriptorSetLayoutViewProjection[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] =
      std::make_shared<DescriptorSetLayout>(_state->getDevice());
  _descriptorSetLayoutViewProjection[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT]->createLightVP(
      VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

  _descriptorSetLayoutDepthTexture = std::make_shared<DescriptorSetLayout>(_state->getDevice());
  _descriptorSetLayoutDepthTexture->createShadowTexture();

  // stub texture
  _stubTexture = std::make_shared<Texture>("../data/Texture1x1.png", _state->getSettings()->getLoadTextureColorFormat(),
                                           VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer,
                                           _state->getDevice());
  _stubCubemap = std::make_shared<Cubemap>("../data/Texture1x1.png", _state->getSettings()->getLoadTextureColorFormat(),
                                           commandBufferTransfer, _state);
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
        resolution, 1, 1, _state->getSettings()->getDepthFormat(), VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        _state->getDevice());
    image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                        VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, _commandBufferTransfer);
    auto imageView = std::make_shared<ImageView>(image, VK_IMAGE_VIEW_TYPE_2D, 1, 0, 1, VK_IMAGE_ASPECT_DEPTH_BIT,
                                                 _state->getDevice());
    depthTexture.push_back(
        std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, imageView, _state->getDevice()));
  }

  auto light = std::make_shared<DirectionalLight>();
  light->setDepthTexture(depthTexture);
  _directionalLights.push_back(light);
  _changed = true;
  return light;
}

std::vector<std::shared_ptr<DirectionalLight>> LightManager::getDirectionalLights() { return _directionalLights; }

std::shared_ptr<DescriptorSetLayout> LightManager::getDSLViewProjection(VkShaderStageFlagBits stage) {
  return _descriptorSetLayoutViewProjection[stage];
}

std::shared_ptr<DescriptorSet> LightManager::getDSViewProjection(VkShaderStageFlagBits stage) {
  return _descriptorSetViewProjection[stage];
}

std::shared_ptr<DescriptorSetLayout> LightManager::getDSLLight() { return _descriptorSetLayoutLight; }

std::shared_ptr<DescriptorSet> LightManager::getDSLight() { return _descriptorSetLight; }

std::shared_ptr<DescriptorSetLayout> LightManager::getDSLShadowTexture() { return _descriptorSetLayoutDepthTexture; }

std::vector<std::shared_ptr<DescriptorSet>> LightManager::getDSShadowTexture() { return _descriptorSetDepthTexture; }

void LightManager::draw(int frame) {
  if (_changed) {
    int size = 0;
    for (int i = 0; i < _directionalLights.size(); i++) {
      size += _directionalLights[i]->getSize();
    }

    if (_directionalLights.size() > 0) {
      _lightDirectionalSSBO.resize(_state->getSettings()->getMaxFramesInFlight());
      for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
        _lightDirectionalSSBO[i] = std::make_shared<Buffer>(
            size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());
      }
    }

    size = 0;
    for (int i = 0; i < _pointLights.size(); i++) {
      size += _pointLights[i]->getSize();
    }

    if (_pointLights.size() > 0) {
      _lightPointSSBO.resize(_state->getSettings()->getMaxFramesInFlight());
      for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
        _lightPointSSBO[i] = std::make_shared<Buffer>(
            size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());
      }
    }

    size = 0;
    for (int i = 0; i < _directionalLights.size(); i++) {
      size += sizeof(glm::mat4);
    }

    if (_directionalLights.size() > 0) {
      _lightDirectionalSSBOViewProjection.resize(_state->getSettings()->getMaxFramesInFlight());
      for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
        _lightDirectionalSSBOViewProjection[i] = std::make_shared<Buffer>(
            size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());
      }
    }

    size = 0;
    for (int i = 0; i < _pointLights.size(); i++) {
      size += sizeof(glm::mat4);
    }

    if (_pointLights.size() > 0) {
      _lightPointSSBOViewProjection.resize(_state->getSettings()->getMaxFramesInFlight());
      for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
        _lightPointSSBOViewProjection[i] = std::make_shared<Buffer>(
            size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());
      }
    }

    _descriptorSetLight = std::make_shared<DescriptorSet>(
        _state->getSettings()->getMaxFramesInFlight(), _descriptorSetLayoutLight, _descriptorPool, _state->getDevice());
    _descriptorSetLight->createLight(_lightDirectionalSSBO, _lightPointSSBO);

    // for models/sprites
    _descriptorSetViewProjection[VK_SHADER_STAGE_VERTEX_BIT] = std::make_shared<DescriptorSet>(
        _state->getSettings()->getMaxFramesInFlight(), _descriptorSetLayoutViewProjection[VK_SHADER_STAGE_VERTEX_BIT],
        _descriptorPool, _state->getDevice());
    _descriptorSetViewProjection[VK_SHADER_STAGE_VERTEX_BIT]->createLight(_lightDirectionalSSBOViewProjection,
                                                                          _lightPointSSBOViewProjection);
    // for terrain
    _descriptorSetViewProjection[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] = std::make_shared<DescriptorSet>(
        _state->getSettings()->getMaxFramesInFlight(),
        _descriptorSetLayoutViewProjection[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT], _descriptorPool,
        _state->getDevice());
    _descriptorSetViewProjection[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT]->createLight(
        _lightDirectionalSSBOViewProjection, _lightPointSSBOViewProjection);

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

  int offset = 0;
  if (_directionalLights.size() > 0) {
    _lightDirectionalSSBO[frame]->map();
    for (int i = 0; i < _directionalLights.size(); i++) {
      // we pass inverse bind matrices to shader via SSBO
      memcpy((uint8_t*)(_lightDirectionalSSBO[frame]->getMappedMemory()) + offset, _directionalLights[i]->getData(),
             _directionalLights[i]->getSize());
      offset += _directionalLights[i]->getSize();
    }
    _lightDirectionalSSBO[frame]->unmap();
  }

  if (_pointLights.size() > 0) {
    _lightPointSSBO[frame]->map();
    offset = 0;
    for (int i = 0; i < _pointLights.size(); i++) {
      // we pass inverse bind matrices to shader via SSBO
      memcpy((uint8_t*)(_lightPointSSBO[frame]->getMappedMemory()) + offset, _pointLights[i]->getData(),
             _pointLights[i]->getSize());
      offset += _pointLights[i]->getSize();
    }
    _lightPointSSBO[frame]->unmap();
  }

  if (_directionalLights.size() > 0) {
    _lightDirectionalSSBOViewProjection[frame]->map();
    std::vector<glm::mat4> directionalVP;
    for (int i = 0; i < _directionalLights.size(); i++) {
      glm::mat4 viewProjection = _directionalLights[i]->getProjectionMatrix() * _directionalLights[i]->getViewMatrix();
      directionalVP.push_back(viewProjection);
    }
    memcpy((uint8_t*)(_lightDirectionalSSBOViewProjection[frame]->getMappedMemory()), directionalVP.data(),
           directionalVP.size() * sizeof(glm::mat4));
    _lightDirectionalSSBOViewProjection[frame]->unmap();
  }

  if (_pointLights.size() > 0) {
    _lightPointSSBOViewProjection[frame]->map();
    std::vector<glm::mat4> pointVP;
    float aspect = (float)std::get<0>(_state->getSettings()->getResolution()) /
                   (float)std::get<1>(_state->getSettings()->getResolution());
    for (int i = 0; i < _pointLights.size(); i++) {
      glm::mat4 viewProjection = _pointLights[i]->getProjectionMatrix() * _pointLights[i]->getViewMatrix(0);
      pointVP.push_back(viewProjection);
    }
    memcpy((uint8_t*)(_lightPointSSBOViewProjection[frame]->getMappedMemory()), pointVP.data(),
           pointVP.size() * sizeof(glm::mat4));
    _lightPointSSBOViewProjection[frame]->unmap();
  }
}