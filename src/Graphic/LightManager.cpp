#include "Graphic/LightManager.h"
#include "Vulkan/Buffer.h"

LightManager::LightManager(std::shared_ptr<ResourceManager> resourceManager, std::shared_ptr<EngineState> engineState) {
  _engineState = engineState;
  _debuggerUtils = std::make_shared<DebuggerUtils>(engineState->getInstance(), engineState->getDevice());

  _descriptorPool = std::make_shared<DescriptorPool>(_descriptorPoolSize, _engineState->getDevice());

  // update global descriptor for Phong and PBR (2 separate)
  {
    _descriptorSetLayoutGlobalPhong = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
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
    _descriptorSetLayoutGlobalPBR = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
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
  {
    _descriptorSetLayoutGlobalTerrainPhong = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
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
    _descriptorSetLayoutGlobalTerrainPBR = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
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

  _lightDirectionalSSBOViewProjection.resize(_engineState->getSettings()->getMaxFramesInFlight());
  _lightPointSSBOViewProjection.resize(_engineState->getSettings()->getMaxFramesInFlight());
  _lightDirectionalSSBO.resize(_engineState->getSettings()->getMaxFramesInFlight());
  _lightPointSSBO.resize(_engineState->getSettings()->getMaxFramesInFlight());
  _lightAmbientSSBO.resize(_engineState->getSettings()->getMaxFramesInFlight());

  {
    auto lightTmp = std::make_shared<DirectionalLight>(_engineState);
    _lightDirectionalSSBOStub = std::make_shared<Buffer>(
        sizeof(glm::vec4) + lightTmp->getSize(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);

    // fill only number of lights because it's stub
    int number = 0;
    _lightDirectionalSSBOStub->setData(&number, sizeof(glm::vec4));
  }
  {
    auto lightTmp = std::make_shared<PointLight>(_engineState);
    _lightPointSSBOStub = std::make_shared<Buffer>(
        sizeof(glm::vec4) + lightTmp->getSize(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);

    // fill only number of lights because it's stub
    int number = 0;
    _lightPointSSBOStub->setData(&number, sizeof(glm::vec4));
  }
  {
    auto lightTmp = std::make_shared<AmbientLight>();
    _lightAmbientSSBOStub = std::make_shared<Buffer>(
        sizeof(glm::vec4) + lightTmp->getSize(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);

    // fill only number of lights because it's stub
    int number = 0;
    _lightAmbientSSBOStub->setData(&number, sizeof(glm::vec4));
  }
  {
    _lightDirectionalSSBOViewProjectionStub = std::make_shared<Buffer>(
        sizeof(glm::vec4) + sizeof(glm::mat4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);

    // fill only number of lights because it's stub
    int number = 0;
    _lightDirectionalSSBOViewProjectionStub->setData(&number, sizeof(glm::vec4));
  }
  {
    _lightPointSSBOViewProjectionStub = std::make_shared<Buffer>(
        sizeof(glm::vec4) + sizeof(glm::mat4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);

    // fill only number of lights because it's stub
    int number = 0;
    _lightPointSSBOViewProjectionStub->setData(&number, sizeof(glm::vec4));
  }

  _descriptorSetGlobalPhong = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                              _descriptorSetLayoutGlobalPhong, _descriptorPool,
                                                              _engineState->getDevice());
  _debuggerUtils->setName("Descriptor set global Phong", VkObjectType::VK_OBJECT_TYPE_DESCRIPTOR_SET,
                          _descriptorSetGlobalPhong->getDescriptorSets());
  _descriptorSetGlobalPBR = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                            _descriptorSetLayoutGlobalPBR, _descriptorPool,
                                                            _engineState->getDevice());
  _debuggerUtils->setName("Descriptor set global PBR", VkObjectType::VK_OBJECT_TYPE_DESCRIPTOR_SET,
                          _descriptorSetGlobalPBR->getDescriptorSets());
  _descriptorSetGlobalTerrainPhong = std::make_shared<DescriptorSet>(
      _engineState->getSettings()->getMaxFramesInFlight(), _descriptorSetLayoutGlobalTerrainPhong, _descriptorPool,
      _engineState->getDevice());
  _debuggerUtils->setName("Descriptor set global terrain Phong", VkObjectType::VK_OBJECT_TYPE_DESCRIPTOR_SET,
                          _descriptorSetGlobalTerrainPhong->getDescriptorSets());
  _descriptorSetGlobalTerrainPBR = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                                   _descriptorSetLayoutGlobalTerrainPBR,
                                                                   _descriptorPool, _engineState->getDevice());
  _debuggerUtils->setName("Descriptor set global terrain PBR", VkObjectType::VK_OBJECT_TYPE_DESCRIPTOR_SET,
                          _descriptorSetGlobalTerrainPBR->getDescriptorSets());

  // stub texture
  _stubTexture = resourceManager->getTextureOne();
  _stubCubemap = resourceManager->getCubemapOne();

  _changed[LightType::DIRECTIONAL].resize(engineState->getSettings()->getMaxFramesInFlight(), false);
  _changed[LightType::POINT].resize(engineState->getSettings()->getMaxFramesInFlight(), false);
  _changed[LightType::AMBIENT].resize(engineState->getSettings()->getMaxFramesInFlight(), false);

  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
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

  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
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
      std::vector<VkDescriptorImageInfo> directionalImageInfo(_engineState->getSettings()->getMaxDirectionalLights());
      for (int j = 0; j < directionalImageInfo.size(); j++) {
        auto texture = _stubTexture;
        if (j < _directionalShadows.size() && _directionalShadows[j])
          texture = _directionalShadows[j]->getShadowMapTexture()[currentFrame];
        directionalImageInfo[j].imageLayout = texture->getImageView()->getImage()->getImageLayout();
        directionalImageInfo[j].imageView = texture->getImageView()->getImageView();
        directionalImageInfo[j].sampler = texture->getSampler()->getSampler();
      }
      textureInfo[4] = directionalImageInfo;

      std::vector<VkDescriptorImageInfo> pointImageInfo(_engineState->getSettings()->getMaxPointLights());
      for (int j = 0; j < pointImageInfo.size(); j++) {
        auto cubemap = _stubCubemap;
        if (j < _pointShadows.size() && _pointShadows[j])
          cubemap = _pointShadows[j]->getShadowMapCubemap()[currentFrame];

        pointImageInfo[j].imageLayout = cubemap->getTexture()->getImageView()->getImage()->getImageLayout();

        pointImageInfo[j].imageView = cubemap->getTexture()->getImageView()->getImageView();
        pointImageInfo[j].sampler = cubemap->getTexture()->getSampler()->getSampler();
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
      std::vector<VkDescriptorImageInfo> directionalImageInfo(_engineState->getSettings()->getMaxDirectionalLights());
      for (int j = 0; j < directionalImageInfo.size(); j++) {
        auto texture = _stubTexture;
        if (j < _directionalShadows.size() && _directionalShadows[j])
          texture = _directionalShadows[j]->getShadowMapTexture()[currentFrame];
        directionalImageInfo[j].imageLayout = texture->getImageView()->getImage()->getImageLayout();
        directionalImageInfo[j].imageView = texture->getImageView()->getImageView();
        directionalImageInfo[j].sampler = texture->getSampler()->getSampler();
      }
      textureInfo[3] = directionalImageInfo;

      std::vector<VkDescriptorImageInfo> pointImageInfo(_engineState->getSettings()->getMaxPointLights());
      for (int j = 0; j < pointImageInfo.size(); j++) {
        auto cubemap = _stubCubemap;
        if (j < _pointShadows.size() && _pointShadows[j])
          cubemap = _pointShadows[j]->getShadowMapCubemap()[currentFrame];

        pointImageInfo[j].imageLayout = cubemap->getTexture()->getImageView()->getImage()->getImageLayout();

        pointImageInfo[j].imageView = cubemap->getTexture()->getImageView()->getImageView();
        pointImageInfo[j].sampler = cubemap->getTexture()->getSampler()->getSampler();
      }
      textureInfo[4] = pointImageInfo;
    }
    _descriptorSetGlobalPBR->createCustom(currentFrame, bufferInfo, textureInfo);
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
      std::vector<VkDescriptorImageInfo> directionalImageInfo(_engineState->getSettings()->getMaxDirectionalLights());
      for (int j = 0; j < directionalImageInfo.size(); j++) {
        auto texture = _stubTexture;
        if (j < _directionalShadows.size() && _directionalShadows[j])
          texture = _directionalShadows[j]->getShadowMapTexture()[currentFrame];
        directionalImageInfo[j].imageLayout = texture->getImageView()->getImage()->getImageLayout();
        directionalImageInfo[j].imageView = texture->getImageView()->getImageView();
        directionalImageInfo[j].sampler = texture->getSampler()->getSampler();
      }
      textureInfo[4] = directionalImageInfo;

      std::vector<VkDescriptorImageInfo> pointImageInfo(_engineState->getSettings()->getMaxPointLights());
      for (int j = 0; j < pointImageInfo.size(); j++) {
        auto cubemap = _stubCubemap;
        if (j < _pointShadows.size() && _pointShadows[j])
          cubemap = _pointShadows[j]->getShadowMapCubemap()[currentFrame];

        pointImageInfo[j].imageLayout = cubemap->getTexture()->getImageView()->getImage()->getImageLayout();

        pointImageInfo[j].imageView = cubemap->getTexture()->getImageView()->getImageView();
        pointImageInfo[j].sampler = cubemap->getTexture()->getSampler()->getSampler();
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
      std::vector<VkDescriptorImageInfo> directionalImageInfo(_engineState->getSettings()->getMaxDirectionalLights());
      for (int j = 0; j < directionalImageInfo.size(); j++) {
        auto texture = _stubTexture;
        if (j < _directionalShadows.size() && _directionalShadows[j])
          texture = _directionalShadows[j]->getShadowMapTexture()[currentFrame];
        directionalImageInfo[j].imageLayout = texture->getImageView()->getImage()->getImageLayout();
        directionalImageInfo[j].imageView = texture->getImageView()->getImageView();
        directionalImageInfo[j].sampler = texture->getSampler()->getSampler();
      }
      textureInfo[3] = directionalImageInfo;

      std::vector<VkDescriptorImageInfo> pointImageInfo(_engineState->getSettings()->getMaxPointLights());
      for (int j = 0; j < pointImageInfo.size(); j++) {
        auto cubemap = _stubCubemap;
        if (j < _pointShadows.size() && _pointShadows[j])
          cubemap = _pointShadows[j]->getShadowMapCubemap()[currentFrame];

        pointImageInfo[j].imageLayout = cubemap->getTexture()->getImageView()->getImage()->getImageLayout();

        pointImageInfo[j].imageView = cubemap->getTexture()->getImageView()->getImageView();
        pointImageInfo[j].sampler = cubemap->getTexture()->getSampler()->getSampler();
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
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);
  }
}

void LightManager::_updateAmbientBuffers(int currentFrame) {
  if (_ambientLights.size() > 0) {
    int offset = 0;
    // we don't want to do multiple map/unmap that's why we don't use corresponding Buffer method setData
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
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);
  }

  size = 0;
  size += sizeof(glm::vec4);
  for (int i = 0; i < _directionalLights.size(); i++) {
    size += sizeof(glm::mat4);
  }

  if (_directionalLights.size() > 0) {
    _lightDirectionalSSBOViewProjection[currentFrame] = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);
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
      glm::mat4 viewProjection = _directionalLights[i]->getCamera()->getProjection() *
                                 _directionalLights[i]->getCamera()->getView();
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

void LightManager::_reallocatePointBuffers(int currentFrame) {
  int size = 0;
  size += sizeof(glm::vec4);
  for (int i = 0; i < _pointLights.size(); i++) {
    size += _pointLights[i]->getSize();
  }

  if (_pointLights.size() > 0) {
    _lightPointSSBO[currentFrame] = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);
  }

  size = 0;
  size += sizeof(glm::vec4);
  for (int i = 0; i < _pointLights.size(); i++) {
    size += sizeof(glm::mat4);
  }

  if (_pointLights.size() > 0) {
    _lightPointSSBOViewProjection[currentFrame] = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);
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
    float aspect = (float)std::get<0>(_engineState->getSettings()->getResolution()) /
                   (float)std::get<1>(_engineState->getSettings()->getResolution());
    for (int i = 0; i < _pointLights.size(); i++) {
      glm::mat4 viewProjection = _pointLights[i]->getCamera()->getProjection() *
                                 _pointLights[i]->getCamera()->getView(0);
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

std::shared_ptr<PointLight> LightManager::createPointLight() {
  auto light = std::make_shared<PointLight>(_engineState);

  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _pointLights.push_back(light);
  _pointShadows.push_back(nullptr);

  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _changed[LightType::POINT][i] = true;
  }

  return light;
}

std::shared_ptr<PointShadow> LightManager::createPointShadow(std::shared_ptr<PointLight> pointLight,
                                                             std::shared_ptr<RenderPass> renderPass,
                                                             std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  auto shadow = std::make_shared<PointShadow>(commandBufferTransfer, renderPass, _debuggerUtils, _engineState);
  int indexLight = std::distance(_pointLights.begin(), find(_pointLights.begin(), _pointLights.end(), pointLight));
  _pointShadows[indexLight] = shadow;

  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _changed[LightType::POINT][i] = true;
  }

  return shadow;
}

void LightManager::removePointLight(std::shared_ptr<PointLight> pointLight) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  auto erased = _pointLights.erase(std::remove(_pointLights.begin(), _pointLights.end(), pointLight),
                                   _pointLights.end());
  if (erased != _pointLights.end()) {
    for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
      _changed[LightType::POINT][i] = true;
    }
  }
}

void LightManager::removePoinShadow(std::shared_ptr<PointShadow> pointShadow) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  auto erased = _pointShadows.erase(std::remove(_pointShadows.begin(), _pointShadows.end(), pointShadow),
                                    _pointShadows.end());
  if (erased != _pointShadows.end()) {
    for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
      _changed[LightType::POINT][i] = true;
    }
  }
}

std::shared_ptr<DirectionalLight> LightManager::createDirectionalLight() {
  auto light = std::make_shared<DirectionalLight>(_engineState);

  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _directionalLights.push_back(light);
  _directionalShadows.push_back(nullptr);

  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _changed[LightType::DIRECTIONAL][i] = true;
  }

  return light;
}

std::shared_ptr<DirectionalShadow> LightManager::createDirectionalShadow(
    std::shared_ptr<DirectionalLight> directionalLight,
    std::shared_ptr<RenderPass> renderPass,
    std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  auto shadow = std::make_shared<DirectionalShadow>(commandBufferTransfer, renderPass, _debuggerUtils, _engineState);
  int indexLight = std::distance(_directionalLights.begin(),
                                 find(_directionalLights.begin(), _directionalLights.end(), directionalLight));
  _directionalShadows[indexLight] = shadow;

  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _changed[LightType::DIRECTIONAL][i] = true;
  }
  return shadow;
}

void LightManager::removeDirectionalLight(std::shared_ptr<DirectionalLight> directionalLight) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  auto erased = _directionalLights.erase(
      std::remove(_directionalLights.begin(), _directionalLights.end(), directionalLight), _directionalLights.end());
  if (erased != _directionalLights.end()) {
    for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
      _changed[LightType::DIRECTIONAL][i] = true;
    }
  }
}

void LightManager::removeDirectionalShadow(std::shared_ptr<DirectionalShadow> directionalShadow) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  auto erased = _directionalShadows.erase(
      std::remove(_directionalShadows.begin(), _directionalShadows.end(), directionalShadow),
      _directionalShadows.end());
  if (erased != _directionalShadows.end()) {
    for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
      _changed[LightType::DIRECTIONAL][i] = true;
    }
  }
}

const std::vector<std::shared_ptr<PointLight>>& LightManager::getPointLights() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _pointLights;
}

const std::vector<std::shared_ptr<PointShadow>>& LightManager::getPointShadows() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _pointShadows;
}

const std::vector<std::shared_ptr<DirectionalLight>>& LightManager::getDirectionalLights() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _directionalLights;
}

const std::vector<std::shared_ptr<DirectionalShadow>>& LightManager::getDirectionalShadows() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _directionalShadows;
}

std::shared_ptr<DescriptorSetLayout> LightManager::getDSLGlobalPhong() { return _descriptorSetLayoutGlobalPhong; }
std::shared_ptr<DescriptorSetLayout> LightManager::getDSLGlobalPBR() { return _descriptorSetLayoutGlobalPBR; }
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
    updateLightDescriptors = true;
  }

  if (_changed[LightType::POINT][currentFrame]) {
    _changed[LightType::POINT][currentFrame] = false;
    _reallocatePointBuffers(currentFrame);
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