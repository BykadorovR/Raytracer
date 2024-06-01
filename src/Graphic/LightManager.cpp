#include "LightManager.h"
#include "Buffer.h"

LightManager::LightManager(std::shared_ptr<CommandBuffer> commandBufferTransfer,
                           std::shared_ptr<ResourceManager> resourceManager,
                           std::shared_ptr<State> state) {
  _state = state;
  _commandBufferTransfer = commandBufferTransfer;

  _descriptorPool = std::make_shared<DescriptorPool>(_descriptorPoolSize, _state->getDevice());

  // update global descriptor for Phong and PBR (2 separate)
  {
    _descriptorSetLayoutGlobalPhong = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutPhong(6);
    layoutPhong[0].binding = 0;
    layoutPhong[0].descriptorCount = 1;
    layoutPhong[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutPhong[0].pImmutableSamplers = nullptr;
    layoutPhong[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layoutPhong[1].binding = 1;
    layoutPhong[1].descriptorCount = 1;
    layoutPhong[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutPhong[1].pImmutableSamplers = nullptr;
    layoutPhong[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPhong[2].binding = 2;
    layoutPhong[2].descriptorCount = 1;
    layoutPhong[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutPhong[2].pImmutableSamplers = nullptr;
    layoutPhong[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPhong[3].binding = 3;
    layoutPhong[3].descriptorCount = 1;
    layoutPhong[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutPhong[3].pImmutableSamplers = nullptr;
    layoutPhong[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPhong[4].binding = 4;
    layoutPhong[4].descriptorCount = 2;
    layoutPhong[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPhong[4].pImmutableSamplers = nullptr;
    layoutPhong[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPhong[5].binding = 5;
    layoutPhong[5].descriptorCount = 4;
    layoutPhong[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPhong[5].pImmutableSamplers = nullptr;
    layoutPhong[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    _descriptorSetLayoutGlobalPhong->createCustom(layoutPhong);
  }
  {
    _descriptorSetLayoutGlobalPBR = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutPBR(5);
    layoutPBR[0].binding = 0;
    layoutPBR[0].descriptorCount = 1;
    layoutPBR[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutPBR[0].pImmutableSamplers = nullptr;
    layoutPBR[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layoutPBR[1].binding = 1;
    layoutPBR[1].descriptorCount = 1;
    layoutPBR[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutPBR[1].pImmutableSamplers = nullptr;
    layoutPBR[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[2].binding = 2;
    layoutPBR[2].descriptorCount = 1;
    layoutPBR[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutPBR[2].pImmutableSamplers = nullptr;
    layoutPBR[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[3].binding = 3;
    layoutPBR[3].descriptorCount = 2;
    layoutPBR[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[3].pImmutableSamplers = nullptr;
    layoutPBR[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[4].binding = 4;
    layoutPBR[4].descriptorCount = 4;
    layoutPBR[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[4].pImmutableSamplers = nullptr;
    layoutPBR[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    _descriptorSetLayoutGlobalPBR->createCustom(layoutPBR);
  }
  // global descriptor set for Terrain color, Phong and PBR (3 different)
  {
    _descriptorSetLayoutGlobalTerrainColor = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutColor(1);
    layoutColor[0].binding = 0;
    layoutColor[0].descriptorCount = 1;
    layoutColor[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutColor[0].pImmutableSamplers = nullptr;
    layoutColor[0].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    _descriptorSetLayoutGlobalTerrainColor->createCustom(layoutColor);
  }
  {
    _descriptorSetLayoutGlobalTerrainPhong = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutPhong(6);
    layoutPhong[0].binding = 0;
    layoutPhong[0].descriptorCount = 1;
    layoutPhong[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutPhong[0].pImmutableSamplers = nullptr;
    layoutPhong[0].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    layoutPhong[1].binding = 1;
    layoutPhong[1].descriptorCount = 1;
    layoutPhong[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutPhong[1].pImmutableSamplers = nullptr;
    layoutPhong[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPhong[2].binding = 2;
    layoutPhong[2].descriptorCount = 1;
    layoutPhong[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutPhong[2].pImmutableSamplers = nullptr;
    layoutPhong[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPhong[3].binding = 3;
    layoutPhong[3].descriptorCount = 1;
    layoutPhong[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutPhong[3].pImmutableSamplers = nullptr;
    layoutPhong[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPhong[4].binding = 4;
    layoutPhong[4].descriptorCount = 2;
    layoutPhong[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPhong[4].pImmutableSamplers = nullptr;
    layoutPhong[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPhong[5].binding = 5;
    layoutPhong[5].descriptorCount = 4;
    layoutPhong[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPhong[5].pImmutableSamplers = nullptr;
    layoutPhong[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    _descriptorSetLayoutGlobalTerrainPhong->createCustom(layoutPhong);
  }
  {
    _descriptorSetLayoutGlobalTerrainPBR = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutPBR(5);
    layoutPBR[0].binding = 0;
    layoutPBR[0].descriptorCount = 1;
    layoutPBR[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutPBR[0].pImmutableSamplers = nullptr;
    layoutPBR[0].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    layoutPBR[1].binding = 1;
    layoutPBR[1].descriptorCount = 1;
    layoutPBR[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutPBR[1].pImmutableSamplers = nullptr;
    layoutPBR[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[2].binding = 2;
    layoutPBR[2].descriptorCount = 1;
    layoutPBR[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutPBR[2].pImmutableSamplers = nullptr;
    layoutPBR[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[3].binding = 3;
    layoutPBR[3].descriptorCount = 2;
    layoutPBR[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[3].pImmutableSamplers = nullptr;
    layoutPBR[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutPBR[4].binding = 4;
    layoutPBR[4].descriptorCount = 4;
    layoutPBR[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutPBR[4].pImmutableSamplers = nullptr;
    layoutPBR[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    _descriptorSetLayoutGlobalTerrainPBR->createCustom(layoutPBR);
  }

  _lightDirectionalSSBOViewProjection.resize(_state->getSettings()->getMaxFramesInFlight());
  _lightPointSSBOViewProjection.resize(_state->getSettings()->getMaxFramesInFlight());
  _lightDirectionalSSBO.resize(_state->getSettings()->getMaxFramesInFlight());
  _lightPointSSBO.resize(_state->getSettings()->getMaxFramesInFlight());
  _lightAmbientSSBO.resize(_state->getSettings()->getMaxFramesInFlight());

  {
    auto lightTmp = std::make_shared<DirectionalLight>();
    _lightDirectionalSSBOStub = std::make_shared<Buffer>(
        lightTmp->getSize() + sizeof(glm::vec4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);

    // fill only number of lights
    int number = 0;
    _lightDirectionalSSBOStub->map();
    memcpy((uint8_t*)(_lightDirectionalSSBOStub->getMappedMemory()), &number, sizeof(glm::vec4));
    _lightDirectionalSSBOStub->unmap();
  }
  {
    auto lightTmp = std::make_shared<PointLight>(_state->getSettings());
    _lightPointSSBOStub = std::make_shared<Buffer>(
        lightTmp->getSize() + sizeof(glm::vec4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);

    int number = 0;
    _lightPointSSBOStub->map();
    // fill only number of lights
    memcpy((uint8_t*)(_lightPointSSBOStub->getMappedMemory()), &number, sizeof(glm::vec4));
    _lightPointSSBOStub->unmap();
  }
  {
    auto lightTmp = std::make_shared<AmbientLight>();
    _lightAmbientSSBOStub = std::make_shared<Buffer>(
        lightTmp->getSize() + sizeof(glm::vec4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);

    int number = 0;
    _lightAmbientSSBOStub->map();
    // fill only number of lights
    memcpy((uint8_t*)(_lightAmbientSSBOStub->getMappedMemory()), &number, sizeof(glm::vec4));
    _lightAmbientSSBOStub->unmap();
  }
  {
    _lightDirectionalSSBOViewProjectionStub = std::make_shared<Buffer>(
        sizeof(glm::mat4) + sizeof(glm::vec4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);

    int number = 0;
    _lightDirectionalSSBOViewProjectionStub->map();
    // fill only number of lights
    memcpy((uint8_t*)(_lightDirectionalSSBOViewProjectionStub->getMappedMemory()), &number, sizeof(glm::vec4));
    _lightDirectionalSSBOViewProjectionStub->unmap();
  }
  {
    _lightPointSSBOViewProjectionStub = std::make_shared<Buffer>(
        sizeof(glm::mat4) + sizeof(glm::vec4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);

    int number = 0;
    _lightPointSSBOViewProjectionStub->map();
    // fill only number of lights
    memcpy((uint8_t*)(_lightPointSSBOViewProjectionStub->getMappedMemory()), &number, sizeof(glm::vec4));
    _lightPointSSBOViewProjectionStub->unmap();
  }

  _descriptorSetGlobalPhong = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                              _descriptorSetLayoutGlobalPhong, _descriptorPool,
                                                              _state->getDevice());
  _descriptorSetGlobalPBR = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                            _descriptorSetLayoutGlobalPBR, _descriptorPool,
                                                            _state->getDevice());
  _descriptorSetGlobalTerrainColor = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                                     _descriptorSetLayoutGlobalTerrainColor,
                                                                     _descriptorPool, _state->getDevice());
  _descriptorSetGlobalTerrainPhong = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                                     _descriptorSetLayoutGlobalTerrainPhong,
                                                                     _descriptorPool, _state->getDevice());
  _descriptorSetGlobalTerrainPBR = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                                   _descriptorSetLayoutGlobalTerrainPBR,
                                                                   _descriptorPool, _state->getDevice());

  // stub texture
  _stubTexture = std::make_shared<Texture>(
      resourceManager->loadImageGPU<uint8_t>({resourceManager->getAssetEnginePath() + "stubs/Texture1x1.png"}),
      _state->getSettings()->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer,
      _state);
  _stubCubemap = std::make_shared<Cubemap>(
      resourceManager->loadImageGPU<uint8_t>(
          std::vector<std::string>(6, resourceManager->getAssetEnginePath() + "stubs/Texture1x1.png")),
      _state->getSettings()->getLoadTextureColorFormat(), 1, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, _state);

  _directionalTextures.resize(state->getSettings()->getMaxDirectionalLights(), _stubTexture);
  _pointTextures.resize(state->getSettings()->getMaxPointLights(), _stubCubemap->getTexture());

  _changed[LightType::DIRECTIONAL].resize(state->getSettings()->getMaxFramesInFlight(), false);
  _changed[LightType::POINT].resize(state->getSettings()->getMaxFramesInFlight(), false);
  _changed[LightType::AMBIENT].resize(state->getSettings()->getMaxFramesInFlight(), false);

  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    _reallocateDirectionalBuffers(i);
    _reallocatePointBuffers(i);
    _reallocateAmbientBuffers(i);
    _updateDirectionalBuffers(i);
    _updatePointDescriptors(i);
    _updateAmbientBuffers(i);
    _setLightDescriptors(i);
  }
}

std::shared_ptr<AmbientLight> LightManager::createAmbientLight() {
  auto light = std::make_shared<AmbientLight>();

  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _ambientLights.push_back(light);

  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    _changed[LightType::AMBIENT][i] = true;
  }

  return light;
}

std::vector<std::shared_ptr<AmbientLight>> LightManager::getAmbientLights() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _ambientLights;
}

void LightManager::_setLightDescriptors(int currentFrame) {
  // global Phong descriptor set
  {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfo;
    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfo;
    {
      std::vector<VkDescriptorBufferInfo> bufferDirectionalInfo(1);
      bufferDirectionalInfo[0].buffer = _lightDirectionalSSBOViewProjectionStub->getData();
      bufferDirectionalInfo[0].offset = 0;
      bufferDirectionalInfo[0].range = _lightDirectionalSSBOViewProjectionStub->getSize();
      if (_lightDirectionalSSBOViewProjection.size() > currentFrame &&
          _lightDirectionalSSBOViewProjection[currentFrame]) {
        bufferDirectionalInfo[0].buffer = _lightDirectionalSSBOViewProjection[currentFrame]->getData();
        bufferDirectionalInfo[0].range = _lightDirectionalSSBOViewProjection[currentFrame]->getSize();
      }
      bufferInfo[0] = bufferDirectionalInfo;
    }
    {
      std::vector<VkDescriptorBufferInfo> bufferDirectionalInfo(1);
      // write stub buffer as default value
      bufferDirectionalInfo[0].buffer = _lightDirectionalSSBOStub->getData();
      bufferDirectionalInfo[0].offset = 0;
      bufferDirectionalInfo[0].range = _lightDirectionalSSBOStub->getSize();
      if (_lightDirectionalSSBO.size() > currentFrame && _lightDirectionalSSBO[currentFrame]) {
        bufferDirectionalInfo[0].buffer = _lightDirectionalSSBO[currentFrame]->getData();
        bufferDirectionalInfo[0].range = _lightDirectionalSSBO[currentFrame]->getSize();
      }
      bufferInfo[1] = bufferDirectionalInfo;
    }
    {
      std::vector<VkDescriptorBufferInfo> bufferPointInfo(1);
      bufferPointInfo[0].buffer = _lightPointSSBOStub->getData();
      bufferPointInfo[0].offset = 0;
      bufferPointInfo[0].range = _lightPointSSBOStub->getSize();
      if (_lightPointSSBO.size() > currentFrame && _lightPointSSBO[currentFrame]) {
        bufferPointInfo[0].buffer = _lightPointSSBO[currentFrame]->getData();
        bufferPointInfo[0].range = _lightPointSSBO[currentFrame]->getSize();
      }
      bufferInfo[2] = bufferPointInfo;
    }
    {
      std::vector<VkDescriptorBufferInfo> bufferAmbientInfo(1);
      bufferAmbientInfo[0].buffer = _lightAmbientSSBOStub->getData();
      bufferAmbientInfo[0].offset = 0;
      bufferAmbientInfo[0].range = _lightAmbientSSBOStub->getSize();
      if (_lightAmbientSSBO.size() > currentFrame && _lightAmbientSSBO[currentFrame]) {
        bufferAmbientInfo[0].buffer = _lightAmbientSSBO[currentFrame]->getData();
        bufferAmbientInfo[0].range = _lightAmbientSSBO[currentFrame]->getSize();
      }
      bufferInfo[3] = bufferAmbientInfo;
    }
    {
      std::vector<VkDescriptorImageInfo> directionalImageInfo(_directionalTextures.size());
      for (int j = 0; j < directionalImageInfo.size(); j++) {
        directionalImageInfo[j].imageLayout = _directionalTextures[j]->getImageView()->getImage()->getImageLayout();

        directionalImageInfo[j].imageView = _directionalTextures[j]->getImageView()->getImageView();
        directionalImageInfo[j].sampler = _directionalTextures[j]->getSampler()->getSampler();
      }
      textureInfo[4] = directionalImageInfo;

      std::vector<VkDescriptorImageInfo> pointImageInfo(_pointTextures.size());
      for (int j = 0; j < pointImageInfo.size(); j++) {
        pointImageInfo[j].imageLayout = _pointTextures[j]->getImageView()->getImage()->getImageLayout();

        pointImageInfo[j].imageView = _pointTextures[j]->getImageView()->getImageView();
        pointImageInfo[j].sampler = _pointTextures[j]->getSampler()->getSampler();
      }
      textureInfo[5] = pointImageInfo;
    }
    _descriptorSetGlobalPhong->createCustom(currentFrame, bufferInfo, textureInfo);
  }
  // global PBR descriptor set
  {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfo;
    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfo;
    {
      std::vector<VkDescriptorBufferInfo> bufferDirectionalInfo(1);
      bufferDirectionalInfo[0].buffer = _lightDirectionalSSBOViewProjectionStub->getData();
      bufferDirectionalInfo[0].offset = 0;
      bufferDirectionalInfo[0].range = _lightDirectionalSSBOViewProjectionStub->getSize();
      if (_lightDirectionalSSBOViewProjection.size() > currentFrame &&
          _lightDirectionalSSBOViewProjection[currentFrame]) {
        bufferDirectionalInfo[0].buffer = _lightDirectionalSSBOViewProjection[currentFrame]->getData();
        bufferDirectionalInfo[0].range = _lightDirectionalSSBOViewProjection[currentFrame]->getSize();
      }
      bufferInfo[0] = bufferDirectionalInfo;
    }
    {
      std::vector<VkDescriptorBufferInfo> bufferDirectionalInfo(1);
      // write stub buffer as default value
      bufferDirectionalInfo[0].buffer = _lightDirectionalSSBOStub->getData();
      bufferDirectionalInfo[0].offset = 0;
      bufferDirectionalInfo[0].range = _lightDirectionalSSBOStub->getSize();
      if (_lightDirectionalSSBO.size() > currentFrame && _lightDirectionalSSBO[currentFrame]) {
        bufferDirectionalInfo[0].buffer = _lightDirectionalSSBO[currentFrame]->getData();
        bufferDirectionalInfo[0].range = _lightDirectionalSSBO[currentFrame]->getSize();
      }
      bufferInfo[1] = bufferDirectionalInfo;
    }
    {
      std::vector<VkDescriptorBufferInfo> bufferPointInfo(1);
      bufferPointInfo[0].buffer = _lightPointSSBOStub->getData();
      bufferPointInfo[0].offset = 0;
      bufferPointInfo[0].range = _lightPointSSBOStub->getSize();
      if (_lightPointSSBO.size() > currentFrame && _lightPointSSBO[currentFrame]) {
        bufferPointInfo[0].buffer = _lightPointSSBO[currentFrame]->getData();
        bufferPointInfo[0].range = _lightPointSSBO[currentFrame]->getSize();
      }
      bufferInfo[2] = bufferPointInfo;
    }
    {
      std::vector<VkDescriptorImageInfo> directionalImageInfo(_directionalTextures.size());
      for (int j = 0; j < directionalImageInfo.size(); j++) {
        directionalImageInfo[j].imageLayout = _directionalTextures[j]->getImageView()->getImage()->getImageLayout();

        directionalImageInfo[j].imageView = _directionalTextures[j]->getImageView()->getImageView();
        directionalImageInfo[j].sampler = _directionalTextures[j]->getSampler()->getSampler();
      }
      textureInfo[3] = directionalImageInfo;

      std::vector<VkDescriptorImageInfo> pointImageInfo(_pointTextures.size());
      for (int j = 0; j < pointImageInfo.size(); j++) {
        pointImageInfo[j].imageLayout = _pointTextures[j]->getImageView()->getImage()->getImageLayout();

        pointImageInfo[j].imageView = _pointTextures[j]->getImageView()->getImageView();
        pointImageInfo[j].sampler = _pointTextures[j]->getSampler()->getSampler();
      }
      textureInfo[4] = pointImageInfo;
    }
    _descriptorSetGlobalPBR->createCustom(currentFrame, bufferInfo, textureInfo);
  }

  // global Terrain Color descriptor set
  {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfo;
    {
      std::vector<VkDescriptorBufferInfo> bufferDirectionalInfo(1);
      bufferDirectionalInfo[0].buffer = _lightDirectionalSSBOViewProjectionStub->getData();
      bufferDirectionalInfo[0].offset = 0;
      bufferDirectionalInfo[0].range = _lightDirectionalSSBOViewProjectionStub->getSize();
      if (_lightDirectionalSSBOViewProjection.size() > currentFrame &&
          _lightDirectionalSSBOViewProjection[currentFrame]) {
        bufferDirectionalInfo[0].buffer = _lightDirectionalSSBOViewProjection[currentFrame]->getData();
        bufferDirectionalInfo[0].range = _lightDirectionalSSBOViewProjection[currentFrame]->getSize();
      }
      bufferInfo[0] = bufferDirectionalInfo;
    }
    _descriptorSetGlobalTerrainColor->createCustom(currentFrame, bufferInfo, {});
  }
  // Terrain global Phong
  {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfo;
    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfo;
    {
      std::vector<VkDescriptorBufferInfo> bufferDirectionalInfo(1);
      bufferDirectionalInfo[0].buffer = _lightDirectionalSSBOViewProjectionStub->getData();
      bufferDirectionalInfo[0].offset = 0;
      bufferDirectionalInfo[0].range = _lightDirectionalSSBOViewProjectionStub->getSize();
      if (_lightDirectionalSSBOViewProjection.size() > currentFrame &&
          _lightDirectionalSSBOViewProjection[currentFrame]) {
        bufferDirectionalInfo[0].buffer = _lightDirectionalSSBOViewProjection[currentFrame]->getData();
        bufferDirectionalInfo[0].range = _lightDirectionalSSBOViewProjection[currentFrame]->getSize();
      }
      bufferInfo[0] = bufferDirectionalInfo;
    }
    {
      std::vector<VkDescriptorBufferInfo> bufferDirectionalInfo(1);
      // write stub buffer as default value
      bufferDirectionalInfo[0].buffer = _lightDirectionalSSBOStub->getData();
      bufferDirectionalInfo[0].offset = 0;
      bufferDirectionalInfo[0].range = _lightDirectionalSSBOStub->getSize();
      if (_lightDirectionalSSBO.size() > currentFrame && _lightDirectionalSSBO[currentFrame]) {
        bufferDirectionalInfo[0].buffer = _lightDirectionalSSBO[currentFrame]->getData();
        bufferDirectionalInfo[0].range = _lightDirectionalSSBO[currentFrame]->getSize();
      }
      bufferInfo[1] = bufferDirectionalInfo;
    }
    {
      std::vector<VkDescriptorBufferInfo> bufferPointInfo(1);
      bufferPointInfo[0].buffer = _lightPointSSBOStub->getData();
      bufferPointInfo[0].offset = 0;
      bufferPointInfo[0].range = _lightPointSSBOStub->getSize();
      if (_lightPointSSBO.size() > currentFrame && _lightPointSSBO[currentFrame]) {
        bufferPointInfo[0].buffer = _lightPointSSBO[currentFrame]->getData();
        bufferPointInfo[0].range = _lightPointSSBO[currentFrame]->getSize();
      }
      bufferInfo[2] = bufferPointInfo;
    }
    {
      std::vector<VkDescriptorBufferInfo> bufferAmbientInfo(1);
      bufferAmbientInfo[0].buffer = _lightAmbientSSBOStub->getData();
      bufferAmbientInfo[0].offset = 0;
      bufferAmbientInfo[0].range = _lightAmbientSSBOStub->getSize();
      if (_lightAmbientSSBO.size() > currentFrame && _lightAmbientSSBO[currentFrame]) {
        bufferAmbientInfo[0].buffer = _lightAmbientSSBO[currentFrame]->getData();
        bufferAmbientInfo[0].range = _lightAmbientSSBO[currentFrame]->getSize();
      }
      bufferInfo[3] = bufferAmbientInfo;
    }
    {
      std::vector<VkDescriptorImageInfo> directionalImageInfo(_directionalTextures.size());
      for (int j = 0; j < directionalImageInfo.size(); j++) {
        directionalImageInfo[j].imageLayout = _directionalTextures[j]->getImageView()->getImage()->getImageLayout();

        directionalImageInfo[j].imageView = _directionalTextures[j]->getImageView()->getImageView();
        directionalImageInfo[j].sampler = _directionalTextures[j]->getSampler()->getSampler();
      }
      textureInfo[4] = directionalImageInfo;

      std::vector<VkDescriptorImageInfo> pointImageInfo(_pointTextures.size());
      for (int j = 0; j < pointImageInfo.size(); j++) {
        pointImageInfo[j].imageLayout = _pointTextures[j]->getImageView()->getImage()->getImageLayout();

        pointImageInfo[j].imageView = _pointTextures[j]->getImageView()->getImageView();
        pointImageInfo[j].sampler = _pointTextures[j]->getSampler()->getSampler();
      }
      textureInfo[5] = pointImageInfo;
    }
    _descriptorSetGlobalTerrainPhong->createCustom(currentFrame, bufferInfo, textureInfo);
  }
  // terrain global PBR descriptor set
  {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfo;
    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfo;
    {
      std::vector<VkDescriptorBufferInfo> bufferDirectionalInfo(1);
      bufferDirectionalInfo[0].buffer = _lightDirectionalSSBOViewProjectionStub->getData();
      bufferDirectionalInfo[0].offset = 0;
      bufferDirectionalInfo[0].range = _lightDirectionalSSBOViewProjectionStub->getSize();
      if (_lightDirectionalSSBOViewProjection.size() > currentFrame &&
          _lightDirectionalSSBOViewProjection[currentFrame]) {
        bufferDirectionalInfo[0].buffer = _lightDirectionalSSBOViewProjection[currentFrame]->getData();
        bufferDirectionalInfo[0].range = _lightDirectionalSSBOViewProjection[currentFrame]->getSize();
      }
      bufferInfo[0] = bufferDirectionalInfo;
    }
    {
      std::vector<VkDescriptorBufferInfo> bufferDirectionalInfo(1);
      // write stub buffer as default value
      bufferDirectionalInfo[0].buffer = _lightDirectionalSSBOStub->getData();
      bufferDirectionalInfo[0].offset = 0;
      bufferDirectionalInfo[0].range = _lightDirectionalSSBOStub->getSize();
      if (_lightDirectionalSSBO.size() > currentFrame && _lightDirectionalSSBO[currentFrame]) {
        bufferDirectionalInfo[0].buffer = _lightDirectionalSSBO[currentFrame]->getData();
        bufferDirectionalInfo[0].range = _lightDirectionalSSBO[currentFrame]->getSize();
      }
      bufferInfo[1] = bufferDirectionalInfo;
    }
    {
      std::vector<VkDescriptorBufferInfo> bufferPointInfo(1);
      bufferPointInfo[0].buffer = _lightPointSSBOStub->getData();
      bufferPointInfo[0].offset = 0;
      bufferPointInfo[0].range = _lightPointSSBOStub->getSize();
      if (_lightPointSSBO.size() > currentFrame && _lightPointSSBO[currentFrame]) {
        bufferPointInfo[0].buffer = _lightPointSSBO[currentFrame]->getData();
        bufferPointInfo[0].range = _lightPointSSBO[currentFrame]->getSize();
      }
      bufferInfo[2] = bufferPointInfo;
    }
    {
      std::vector<VkDescriptorImageInfo> directionalImageInfo(_directionalTextures.size());
      for (int j = 0; j < directionalImageInfo.size(); j++) {
        directionalImageInfo[j].imageLayout = _directionalTextures[j]->getImageView()->getImage()->getImageLayout();

        directionalImageInfo[j].imageView = _directionalTextures[j]->getImageView()->getImageView();
        directionalImageInfo[j].sampler = _directionalTextures[j]->getSampler()->getSampler();
      }
      textureInfo[3] = directionalImageInfo;

      std::vector<VkDescriptorImageInfo> pointImageInfo(_pointTextures.size());
      for (int j = 0; j < pointImageInfo.size(); j++) {
        pointImageInfo[j].imageLayout = _pointTextures[j]->getImageView()->getImage()->getImageLayout();

        pointImageInfo[j].imageView = _pointTextures[j]->getImageView()->getImageView();
        pointImageInfo[j].sampler = _pointTextures[j]->getSampler()->getSampler();
      }
      textureInfo[4] = pointImageInfo;
    }
    _descriptorSetGlobalTerrainPBR->createCustom(currentFrame, bufferInfo, textureInfo);
  }
}

void LightManager::_reallocateAmbientBuffers(int currentFrame) {
  int size = 0;
  size += sizeof(glm::vec4);
  for (int i = 0; i < _ambientLights.size(); i++) {
    size += _ambientLights[i]->getSize();
  }

  if (_ambientLights.size() > 0) {
    _lightAmbientSSBO[currentFrame] = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);
  }
}

void LightManager::_updateAmbientBuffers(int currentFrame) {
  if (_ambientLights.size() > 0) {
    int offset = 0;
    _lightAmbientSSBO[currentFrame]->map();
    int ambientNumber = _ambientLights.size();
    memcpy((uint8_t*)(_lightAmbientSSBO[currentFrame]->getMappedMemory()) + offset, &ambientNumber, sizeof(glm::vec4));
    offset += sizeof(glm::vec4);
    for (int i = 0; i < _ambientLights.size(); i++) {
      // we pass inverse bind matrices to shader via SSBO
      memcpy((uint8_t*)(_lightAmbientSSBO[currentFrame]->getMappedMemory()) + offset, _ambientLights[i]->getData(),
             _ambientLights[i]->getSize());
      offset += _ambientLights[i]->getSize();
    }
    _lightAmbientSSBO[currentFrame]->unmap();
  }
}

void LightManager::_reallocateDirectionalBuffers(int currentFrame) {
  int size = 0;
  // align is 16 bytes, so even for int because in our SSBO struct
  // we have fields 16 bytes size so the whole struct has 16 bytes allignment
  size += sizeof(glm::vec4);
  for (int i = 0; i < _directionalLights.size(); i++) {
    size += _directionalLights[i]->getSize();
  }

  if (_directionalLights.size() > 0) {
    _lightDirectionalSSBO[currentFrame] = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);
  }

  size = 0;
  size += sizeof(glm::vec4);
  for (int i = 0; i < _directionalLights.size(); i++) {
    size += sizeof(glm::mat4);
  }

  if (_directionalLights.size() > 0) {
    _lightDirectionalSSBOViewProjection[currentFrame] = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);
  }
}

void LightManager::_updateDirectionalBuffers(int currentFrame) {
  int directionalNumber = _directionalLights.size();
  if (_directionalLights.size() > 0) {
    int offset = 0;
    _lightDirectionalSSBO[currentFrame]->map();
    memcpy((uint8_t*)(_lightDirectionalSSBO[currentFrame]->getMappedMemory()) + offset, &directionalNumber,
           sizeof(glm::vec4));
    offset += sizeof(glm::vec4);
    for (int i = 0; i < _directionalLights.size(); i++) {
      // we pass inverse bind matrices to shader via SSBO
      memcpy((uint8_t*)(_lightDirectionalSSBO[currentFrame]->getMappedMemory()) + offset,
             _directionalLights[i]->getData(), _directionalLights[i]->getSize());
      offset += _directionalLights[i]->getSize();
    }
    _lightDirectionalSSBO[currentFrame]->unmap();
  }

  if (_directionalLights.size() > 0) {
    _lightDirectionalSSBOViewProjection[currentFrame]->map();
    std::vector<glm::mat4> directionalVP;
    for (int i = 0; i < _directionalLights.size(); i++) {
      glm::mat4 viewProjection = _directionalLights[i]->getProjectionMatrix() * _directionalLights[i]->getViewMatrix();
      directionalVP.push_back(viewProjection);
    }
    int offset = 0;
    memcpy((uint8_t*)(_lightDirectionalSSBOViewProjection[currentFrame]->getMappedMemory()) + offset,
           &directionalNumber, sizeof(glm::vec4));
    offset += sizeof(glm::vec4);
    memcpy((uint8_t*)(_lightDirectionalSSBOViewProjection[currentFrame]->getMappedMemory()) + offset,
           directionalVP.data(), directionalVP.size() * sizeof(glm::mat4));
    _lightDirectionalSSBOViewProjection[currentFrame]->unmap();
  }
}

void LightManager::_updateDirectionalTexture(int currentFrame) {
  int currentIndex = _directionalLights.size() - 1;
  _directionalTextures[currentIndex] = _directionalLights[currentIndex]->getDepthTexture()[currentFrame];
}

void LightManager::_reallocatePointBuffers(int currentFrame) {
  int size = 0;
  size += sizeof(glm::vec4);
  for (int i = 0; i < _pointLights.size(); i++) {
    size += _pointLights[i]->getSize();
  }

  if (_pointLights.size() > 0) {
    _lightPointSSBO[currentFrame] = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);
  }

  size = 0;
  size += sizeof(glm::vec4);
  for (int i = 0; i < _pointLights.size(); i++) {
    size += sizeof(glm::mat4);
  }

  if (_pointLights.size() > 0) {
    _lightPointSSBOViewProjection[currentFrame] = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);
  }
}

void LightManager::_updatePointDescriptors(int currentFrame) {
  int pointNumber = _pointLights.size();
  if (_pointLights.size() > 0) {
    _lightPointSSBO[currentFrame]->map();
    int offset = 0;
    memcpy((uint8_t*)(_lightPointSSBO[currentFrame]->getMappedMemory()) + offset, &pointNumber, sizeof(glm::vec4));
    offset += sizeof(glm::vec4);
    for (int i = 0; i < _pointLights.size(); i++) {
      // we pass inverse bind matrices to shader via SSBO
      memcpy((uint8_t*)(_lightPointSSBO[currentFrame]->getMappedMemory()) + offset, _pointLights[i]->getData(),
             _pointLights[i]->getSize());
      offset += _pointLights[i]->getSize();
    }
    _lightPointSSBO[currentFrame]->unmap();
  }

  if (_pointLights.size() > 0) {
    _lightPointSSBOViewProjection[currentFrame]->map();
    std::vector<glm::mat4> pointVP;
    float aspect = (float)std::get<0>(_state->getSettings()->getResolution()) /
                   (float)std::get<1>(_state->getSettings()->getResolution());
    for (int i = 0; i < _pointLights.size(); i++) {
      glm::mat4 viewProjection = _pointLights[i]->getProjectionMatrix() * _pointLights[i]->getViewMatrix(0);
      pointVP.push_back(viewProjection);
    }
    int offset = 0;
    memcpy((uint8_t*)(_lightPointSSBOViewProjection[currentFrame]->getMappedMemory()) + offset, &pointNumber,
           sizeof(glm::vec4));
    offset += sizeof(glm::vec4);
    memcpy((uint8_t*)(_lightPointSSBOViewProjection[currentFrame]->getMappedMemory()) + offset, pointVP.data(),
           pointVP.size() * sizeof(glm::mat4));
    _lightPointSSBOViewProjection[currentFrame]->unmap();
  }
}

void LightManager::_updatePointTexture(int currentFrame) {
  for (int i = 0; i < _pointLights.size(); i++)
    _pointTextures[i] = _pointLights[i]->getDepthCubemap()[currentFrame]->getTexture();
}

std::shared_ptr<PointLight> LightManager::createPointLight(std::tuple<int, int> resolution) {
  std::vector<std::shared_ptr<Cubemap>> depthCubemap;
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    depthCubemap.push_back(std::make_shared<Cubemap>(
        resolution, _state->getSettings()->getDepthFormat(), 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        _commandBufferTransfer, _state));
  }

  auto light = std::make_shared<PointLight>(_state->getSettings());
  light->setDepthCubemap(depthCubemap);

  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _pointLights.push_back(light);

  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    _changed[LightType::POINT][i] = true;
  }

  // should be unique command pool for every command buffer to work in parallel.
  // the same with logger, it's binded to command buffer (//TODO: maybe fix somehow)
  std::vector<std::shared_ptr<CommandBuffer>> commandBuffer(6);
  std::vector<std::shared_ptr<LoggerGPU>> loggerGPU(6);
  for (int j = 0; j < commandBuffer.size(); j++) {
    auto commandPool = std::make_shared<CommandPool>(QueueType::GRAPHIC, _state->getDevice());
    commandBuffer[j] = std::make_shared<CommandBuffer>(_state->getSettings()->getMaxFramesInFlight(), commandPool,
                                                       _state);
    loggerGPU[j] = std::make_shared<LoggerGPU>(_state);
  }
  _commandBufferPoint.push_back(commandBuffer);
  _loggerGPUPoint.push_back(loggerGPU);

  return light;
}

void LightManager::removePointLights(std::shared_ptr<PointLight> pointLight) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  int index = -1;
  for (int i = 0; i < _pointLights.size(); i++) {
    if (_pointLights[i] == pointLight) index = i;
  }
  if (index > 0) {
    _pointLights.erase(_pointLights.begin() + index);
    _commandBufferPoint.erase(_commandBufferPoint.begin() + index);
    _loggerGPUPoint.erase(_loggerGPUPoint.begin() + index);
    for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
      _changed[LightType::POINT][i] = true;
    }
  }
}

void LightManager::removeDirectionalLight(std::shared_ptr<DirectionalLight> directionalLight) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  int index = -1;
  for (int i = 0; i < _directionalLights.size(); i++) {
    if (_directionalLights[i] == directionalLight) index = i;
  }
  if (index > 0) {
    _directionalLights.erase(_directionalLights.begin() + index);
    _commandBufferDirectional.erase(_commandBufferDirectional.begin() + index);
    _loggerGPUDirectional.erase(_loggerGPUDirectional.begin() + index);
    for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
      _changed[LightType::DIRECTIONAL][i] = true;
    }
  }
}

const std::vector<std::shared_ptr<PointLight>>& LightManager::getPointLights() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _pointLights;
}

const std::vector<std::vector<std::shared_ptr<CommandBuffer>>>& LightManager::getPointLightCommandBuffers() {
  return _commandBufferPoint;
}

const std::vector<std::vector<std::shared_ptr<LoggerGPU>>>& LightManager::getPointLightLoggers() {
  return _loggerGPUPoint;
}

std::shared_ptr<DirectionalLight> LightManager::createDirectionalLight(std::tuple<int, int> resolution) {
  std::vector<std::shared_ptr<Texture>> depthTexture;
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    std::shared_ptr<Image> image = std::make_shared<Image>(
        resolution, 1, 1, _state->getSettings()->getDepthFormat(), VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        _state);
    image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                        VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, _commandBufferTransfer);
    auto imageView = std::make_shared<ImageView>(image, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1, VK_IMAGE_ASPECT_DEPTH_BIT,
                                                 _state);
    depthTexture.push_back(std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, imageView, _state));
  }

  auto light = std::make_shared<DirectionalLight>();
  light->setDepthTexture(depthTexture);

  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _directionalLights.push_back(light);

  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    _changed[LightType::DIRECTIONAL][i] = true;
  }

  auto commandPool = std::make_shared<CommandPool>(QueueType::GRAPHIC, _state->getDevice());
  _commandBufferDirectional.push_back(
      std::make_shared<CommandBuffer>(_state->getSettings()->getMaxFramesInFlight(), commandPool, _state));
  _loggerGPUDirectional.push_back(std::make_shared<LoggerGPU>(_state));

  return light;
}

const std::vector<std::shared_ptr<DirectionalLight>>& LightManager::getDirectionalLights() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _directionalLights;
}

const std::vector<std::shared_ptr<CommandBuffer>>& LightManager::getDirectionalLightCommandBuffers() {
  return _commandBufferDirectional;
}

const std::vector<std::shared_ptr<LoggerGPU>>& LightManager::getDirectionalLightLoggers() {
  return _loggerGPUDirectional;
}

std::shared_ptr<DescriptorSetLayout> LightManager::getDSLGlobalPhong() { return _descriptorSetLayoutGlobalPhong; }
std::shared_ptr<DescriptorSetLayout> LightManager::getDSLGlobalPBR() { return _descriptorSetLayoutGlobalPBR; }
std::shared_ptr<DescriptorSetLayout> LightManager::getDSLGlobalTerrainColor() {
  return _descriptorSetLayoutGlobalTerrainColor;
}
std::shared_ptr<DescriptorSetLayout> LightManager::getDSLGlobalTerrainPhong() {
  return _descriptorSetLayoutGlobalTerrainPhong;
}
std::shared_ptr<DescriptorSetLayout> LightManager::getDSLGlobalTerrainPBR() {
  return _descriptorSetLayoutGlobalTerrainPBR;
}

std::shared_ptr<DescriptorSet> LightManager::getDSGlobalPhong() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _descriptorSetGlobalPhong;
}

std::shared_ptr<DescriptorSet> LightManager::getDSGlobalPBR() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _descriptorSetGlobalPBR;
}

std::shared_ptr<DescriptorSet> LightManager::getDSGlobalTerrainColor() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _descriptorSetGlobalTerrainColor;
}

std::shared_ptr<DescriptorSet> LightManager::getDSGlobalTerrainPhong() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _descriptorSetGlobalTerrainPhong;
}

std::shared_ptr<DescriptorSet> LightManager::getDSGlobalTerrainPBR() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _descriptorSetGlobalTerrainPBR;
}

void LightManager::draw(int currentFrame) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);

  // change lights configuration on demand only
  float updateLightDescriptors = false;
  if (_changed[LightType::DIRECTIONAL][currentFrame]) {
    _changed[LightType::DIRECTIONAL][currentFrame] = false;
    _reallocateDirectionalBuffers(currentFrame);
    _updateDirectionalTexture(currentFrame);
    updateLightDescriptors = true;
  }

  if (_changed[LightType::POINT][currentFrame]) {
    _changed[LightType::POINT][currentFrame] = false;
    _reallocatePointBuffers(currentFrame);
    _updatePointTexture(currentFrame);
    updateLightDescriptors = true;
  }

  if (_changed[LightType::AMBIENT][currentFrame]) {
    _changed[LightType::AMBIENT][currentFrame] = false;
    _reallocateAmbientBuffers(currentFrame);
    updateLightDescriptors = true;
  }

  // light parameters can be changed on per-frame basis
  _updateDirectionalBuffers(currentFrame);
  _updatePointDescriptors(currentFrame);
  _updateAmbientBuffers(currentFrame);

  if (updateLightDescriptors) _setLightDescriptors(currentFrame);
}