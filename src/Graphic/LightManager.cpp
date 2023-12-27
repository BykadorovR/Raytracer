#include "LightManager.h"
#include "Buffer.h"

LightManager::LightManager(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state) {
  _state = state;
  _commandBufferTransfer = commandBufferTransfer;

  _descriptorPool = std::make_shared<DescriptorPool>(_descriptorPoolSize, _state->getDevice());

  // create descriptor layouts for light info for direct, point and ambient lights for fragment shaders
  {
    _descriptorSetLayoutLight = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutLight(3);
    layoutLight[0].binding = 0;
    layoutLight[0].descriptorCount = 1;
    layoutLight[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutLight[0].pImmutableSamplers = nullptr;
    layoutLight[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutLight[1].binding = 1;
    layoutLight[1].descriptorCount = 1;
    layoutLight[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutLight[1].pImmutableSamplers = nullptr;
    layoutLight[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutLight[2].binding = 2;
    layoutLight[2].descriptorCount = 1;
    layoutLight[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutLight[2].pImmutableSamplers = nullptr;
    layoutLight[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    _descriptorSetLayoutLight->createCustom(layoutLight);
  }
  // create descriptor layouts for VP matrices for direct and points light for vertex shader
  {
    _descriptorSetLayoutViewProjection[VK_SHADER_STAGE_VERTEX_BIT] = std::make_shared<DescriptorSetLayout>(
        _state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutLightVP(2);
    layoutLightVP[0].binding = 0;
    layoutLightVP[0].descriptorCount = 1;
    layoutLightVP[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutLightVP[0].pImmutableSamplers = nullptr;
    layoutLightVP[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layoutLightVP[1].binding = 1;
    layoutLightVP[1].descriptorCount = 1;
    layoutLightVP[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutLightVP[1].pImmutableSamplers = nullptr;
    layoutLightVP[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    _descriptorSetLayoutViewProjection[VK_SHADER_STAGE_VERTEX_BIT]->createCustom(layoutLightVP);
  }

  // create descriptor layouts for VP matrices for direct and points light for tesselation shader
  {
    _descriptorSetLayoutViewProjection[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] =
        std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutLightVP(2);
    layoutLightVP[0].binding = 0;
    layoutLightVP[0].descriptorCount = 1;
    layoutLightVP[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutLightVP[0].pImmutableSamplers = nullptr;
    layoutLightVP[0].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    layoutLightVP[1].binding = 1;
    layoutLightVP[1].descriptorCount = 1;
    layoutLightVP[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutLightVP[1].pImmutableSamplers = nullptr;
    layoutLightVP[1].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    _descriptorSetLayoutViewProjection[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT]->createCustom(layoutLightVP);
  }

  _descriptorSetLayoutDepthTexture = std::make_shared<DescriptorSetLayout>(_state->getDevice());
  _descriptorSetLayoutDepthTexture->createShadowTexture();

  _lightDirectionalSSBOViewProjection.resize(_state->getSettings()->getMaxFramesInFlight());
  _lightPointSSBOViewProjection.resize(_state->getSettings()->getMaxFramesInFlight());
  _lightDirectionalSSBO.resize(_state->getSettings()->getMaxFramesInFlight());
  _lightPointSSBO.resize(_state->getSettings()->getMaxFramesInFlight());
  _lightAmbientSSBO.resize(_state->getSettings()->getMaxFramesInFlight());

  _descriptorSetLight = std::make_shared<DescriptorSet>(
      _state->getSettings()->getMaxFramesInFlight(), _descriptorSetLayoutLight, _descriptorPool, _state->getDevice());

  // for models/sprites
  _descriptorSetViewProjection[VK_SHADER_STAGE_VERTEX_BIT] = std::make_shared<DescriptorSet>(
      _state->getSettings()->getMaxFramesInFlight(), _descriptorSetLayoutViewProjection[VK_SHADER_STAGE_VERTEX_BIT],
      _descriptorPool, _state->getDevice());

  // for terrain
  _descriptorSetViewProjection[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] = std::make_shared<DescriptorSet>(
      _state->getSettings()->getMaxFramesInFlight(),
      _descriptorSetLayoutViewProjection[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT], _descriptorPool,
      _state->getDevice());

  // TODO: make 2 textures in total instead of 4
  _descriptorSetDepthTexture.resize(_state->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    _descriptorSetDepthTexture[i] = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                                    _descriptorSetLayoutDepthTexture, _descriptorPool,
                                                                    _state->getDevice());
  }

  // stub texture
  _stubTexture = std::make_shared<Texture>("assets/stubs/Texture1x1.png",
                                           _state->getSettings()->getLoadTextureColorFormat(),
                                           VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer, _state);
  _stubCubemap = std::make_shared<Cubemap>(
      std::vector<std::string>(6, "assets/stubs/Texture1x1.png"), _state->getSettings()->getLoadTextureColorFormat(), 1,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, _state);

  _directionalTextures.resize(state->getSettings()->getMaxDirectionalLights(), _stubTexture);
  _pointTextures.resize(state->getSettings()->getMaxPointLights(), _stubCubemap->getTexture());

  _changed[LightType::DIRECTIONAL].resize(state->getSettings()->getMaxFramesInFlight(), false);
  _changed[LightType::POINT].resize(state->getSettings()->getMaxFramesInFlight(), false);
  _changed[LightType::AMBIENT].resize(state->getSettings()->getMaxFramesInFlight(), false);

  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    _reallocateDirectionalDescriptors(i);
    _reallocatePointDescriptors(i);
    _reallocateAmbientDescriptors(i);
    _updateDirectionalDescriptors(i);
    _updatePointDescriptors(i);
    _updateAmbientDescriptors(i);
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
  // light info descriptor sets
  {
    std::map<int, VkDescriptorBufferInfo> bufferInfo;
    VkDescriptorBufferInfo bufferDirectionalInfo{};
    bufferDirectionalInfo.buffer = VK_NULL_HANDLE;
    bufferDirectionalInfo.offset = 0;
    bufferDirectionalInfo.range = VK_WHOLE_SIZE;
    if (_lightDirectionalSSBO.size() > currentFrame && _lightDirectionalSSBO[currentFrame]) {
      bufferDirectionalInfo.buffer = _lightDirectionalSSBO[currentFrame]->getData();
      bufferDirectionalInfo.range = _lightDirectionalSSBO[currentFrame]->getSize();
    }
    bufferInfo[0] = bufferDirectionalInfo;

    VkDescriptorBufferInfo bufferPointInfo{};
    bufferPointInfo.buffer = VK_NULL_HANDLE;
    bufferPointInfo.offset = 0;
    bufferPointInfo.range = VK_WHOLE_SIZE;
    if (_lightPointSSBO.size() > currentFrame && _lightPointSSBO[currentFrame]) {
      bufferPointInfo.buffer = _lightPointSSBO[currentFrame]->getData();
      bufferPointInfo.range = _lightPointSSBO[currentFrame]->getSize();
    }
    bufferInfo[1] = bufferPointInfo;

    VkDescriptorBufferInfo bufferAmbientInfo{};
    bufferAmbientInfo.buffer = VK_NULL_HANDLE;
    bufferAmbientInfo.offset = 0;
    bufferAmbientInfo.range = VK_WHOLE_SIZE;
    if (_lightAmbientSSBO.size() > currentFrame && _lightAmbientSSBO[currentFrame]) {
      bufferAmbientInfo.buffer = _lightAmbientSSBO[currentFrame]->getData();
      bufferAmbientInfo.range = _lightAmbientSSBO[currentFrame]->getSize();
    }
    bufferInfo[2] = bufferAmbientInfo;
    _descriptorSetLight->createCustom(currentFrame, bufferInfo, {});
  }

  // light VP matrices for models/sprites
  {
    std::map<int, VkDescriptorBufferInfo> bufferInfo;
    VkDescriptorBufferInfo bufferDirectionalInfo{};
    bufferDirectionalInfo.buffer = VK_NULL_HANDLE;
    bufferDirectionalInfo.offset = 0;
    bufferDirectionalInfo.range = VK_WHOLE_SIZE;
    if (_lightDirectionalSSBOViewProjection.size() > currentFrame &&
        _lightDirectionalSSBOViewProjection[currentFrame]) {
      bufferDirectionalInfo.buffer = _lightDirectionalSSBOViewProjection[currentFrame]->getData();
      bufferDirectionalInfo.range = _lightDirectionalSSBOViewProjection[currentFrame]->getSize();
    }
    bufferInfo[0] = bufferDirectionalInfo;

    VkDescriptorBufferInfo bufferPointInfo{};
    bufferPointInfo.buffer = VK_NULL_HANDLE;
    bufferPointInfo.offset = 0;
    bufferPointInfo.range = VK_WHOLE_SIZE;
    if (_lightPointSSBOViewProjection.size() > currentFrame && _lightPointSSBOViewProjection[currentFrame]) {
      bufferPointInfo.buffer = _lightPointSSBOViewProjection[currentFrame]->getData();
      bufferPointInfo.range = _lightPointSSBOViewProjection[currentFrame]->getSize();
    }
    bufferInfo[1] = bufferPointInfo;

    _descriptorSetViewProjection[VK_SHADER_STAGE_VERTEX_BIT]->createCustom(currentFrame, bufferInfo, {});
  }

  // light VP matrices for terrain
  {
    std::map<int, VkDescriptorBufferInfo> bufferInfo;
    VkDescriptorBufferInfo bufferDirectionalInfo{};
    bufferDirectionalInfo.buffer = VK_NULL_HANDLE;
    bufferDirectionalInfo.offset = 0;
    bufferDirectionalInfo.range = VK_WHOLE_SIZE;
    if (_lightDirectionalSSBOViewProjection.size() > currentFrame &&
        _lightDirectionalSSBOViewProjection[currentFrame]) {
      bufferDirectionalInfo.buffer = _lightDirectionalSSBOViewProjection[currentFrame]->getData();
      bufferDirectionalInfo.range = _lightDirectionalSSBOViewProjection[currentFrame]->getSize();
    }
    bufferInfo[0] = bufferDirectionalInfo;

    VkDescriptorBufferInfo bufferPointInfo{};
    bufferPointInfo.buffer = VK_NULL_HANDLE;
    bufferPointInfo.offset = 0;
    bufferPointInfo.range = VK_WHOLE_SIZE;
    if (_lightPointSSBOViewProjection.size() > currentFrame && _lightPointSSBOViewProjection[currentFrame]) {
      bufferPointInfo.buffer = _lightPointSSBOViewProjection[currentFrame]->getData();
      bufferPointInfo.range = _lightPointSSBOViewProjection[currentFrame]->getSize();
    }
    bufferInfo[1] = bufferPointInfo;

    _descriptorSetViewProjection[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT]->createCustom(currentFrame, bufferInfo,
                                                                                            {});
  }

  _descriptorSetDepthTexture[currentFrame]->createShadowTexture(_directionalTextures, _pointTextures);
}

void LightManager::_reallocateAmbientDescriptors(int currentFrame) {
  int size = 0;
  for (int i = 0; i < _ambientLights.size(); i++) {
    size += _ambientLights[i]->getSize();
  }

  if (_ambientLights.size() > 0) {
    _lightAmbientSSBO[currentFrame] = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());
  }
}

void LightManager::_updateAmbientDescriptors(int currentFrame) {
  if (_ambientLights.size() > 0) {
    int offset = 0;
    _lightAmbientSSBO[currentFrame]->map();
    for (int i = 0; i < _ambientLights.size(); i++) {
      // we pass inverse bind matrices to shader via SSBO
      memcpy((uint8_t*)(_lightAmbientSSBO[currentFrame]->getMappedMemory()) + offset, _ambientLights[i]->getData(),
             _ambientLights[i]->getSize());
      offset += _ambientLights[i]->getSize();
    }
    _lightAmbientSSBO[currentFrame]->unmap();
  }
}

void LightManager::_reallocateDirectionalDescriptors(int currentFrame) {
  int size = 0;
  for (int i = 0; i < _directionalLights.size(); i++) {
    size += _directionalLights[i]->getSize();
  }

  if (_directionalLights.size() > 0) {
    _lightDirectionalSSBO[currentFrame] = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());
  }

  size = 0;
  for (int i = 0; i < _directionalLights.size(); i++) {
    size += sizeof(glm::mat4);
  }

  if (_directionalLights.size() > 0) {
    _lightDirectionalSSBOViewProjection[currentFrame] = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());
  }
}

void LightManager::_updateDirectionalDescriptors(int currentFrame) {
  if (_directionalLights.size() > 0) {
    int offset = 0;
    _lightDirectionalSSBO[currentFrame]->map();
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
    memcpy((uint8_t*)(_lightDirectionalSSBOViewProjection[currentFrame]->getMappedMemory()), directionalVP.data(),
           directionalVP.size() * sizeof(glm::mat4));
    _lightDirectionalSSBOViewProjection[currentFrame]->unmap();
  }
}

void LightManager::_updateDirectionalTexture(int currentFrame) {
  int currentIndex = _directionalLights.size() - 1;
  _directionalTextures[currentIndex] = _directionalLights[currentIndex]->getDepthTexture()[currentFrame];
}

void LightManager::_reallocatePointDescriptors(int currentFrame) {
  int size = 0;
  for (int i = 0; i < _pointLights.size(); i++) {
    size += _pointLights[i]->getSize();
  }

  if (_pointLights.size() > 0) {
    _lightPointSSBO[currentFrame] = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());
  }

  size = 0;
  for (int i = 0; i < _pointLights.size(); i++) {
    size += sizeof(glm::mat4);
  }

  if (_pointLights.size() > 0) {
    _lightPointSSBOViewProjection[currentFrame] = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());
  }
}

void LightManager::_updatePointDescriptors(int currentFrame) {
  if (_pointLights.size() > 0) {
    _lightPointSSBO[currentFrame]->map();
    int offset = 0;
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
    memcpy((uint8_t*)(_lightPointSSBOViewProjection[currentFrame]->getMappedMemory()), pointVP.data(),
           pointVP.size() * sizeof(glm::mat4));
    _lightPointSSBOViewProjection[currentFrame]->unmap();
  }
}

void LightManager::_updatePointTexture(int currentFrame) {
  int currentIndex = _pointLights.size() - 1;
  _pointTextures[currentIndex] = _pointLights[currentIndex]->getDepthCubemap()[currentFrame]->getTexture();
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

  return light;
}

std::vector<std::shared_ptr<PointLight>> LightManager::getPointLights() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _pointLights;
}

std::shared_ptr<DirectionalLight> LightManager::createDirectionalLight(std::tuple<int, int> resolution) {
  std::vector<std::shared_ptr<Texture>> depthTexture;
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    std::shared_ptr<Image> image = std::make_shared<Image>(
        resolution, 1, 1, _state->getSettings()->getDepthFormat(), VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        _state->getDevice());
    image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                        VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, _commandBufferTransfer);
    auto imageView = std::make_shared<ImageView>(image, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1, VK_IMAGE_ASPECT_DEPTH_BIT,
                                                 _state->getDevice());
    depthTexture.push_back(std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, imageView, _state));
  }

  auto light = std::make_shared<DirectionalLight>();
  light->setDepthTexture(depthTexture);

  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _directionalLights.push_back(light);

  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    _changed[LightType::DIRECTIONAL][i] = true;
  }

  return light;
}

std::vector<std::shared_ptr<DirectionalLight>> LightManager::getDirectionalLights() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _directionalLights;
}

std::shared_ptr<DescriptorSetLayout> LightManager::getDSLViewProjection(VkShaderStageFlagBits stage) {
  return _descriptorSetLayoutViewProjection[stage];
}

std::shared_ptr<DescriptorSet> LightManager::getDSViewProjection(VkShaderStageFlagBits stage) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _descriptorSetViewProjection[stage];
}

std::shared_ptr<DescriptorSetLayout> LightManager::getDSLLight() { return _descriptorSetLayoutLight; }

std::shared_ptr<DescriptorSet> LightManager::getDSLight() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _descriptorSetLight;
}

std::shared_ptr<DescriptorSetLayout> LightManager::getDSLShadowTexture() { return _descriptorSetLayoutDepthTexture; }

std::vector<std::shared_ptr<DescriptorSet>> LightManager::getDSShadowTexture() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _descriptorSetDepthTexture;
}

void LightManager::draw(int currentFrame) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);

  // change lights configuration on demand only
  float updateLightDescriptors = false;
  if (_changed[LightType::DIRECTIONAL][currentFrame]) {
    _changed[LightType::DIRECTIONAL][currentFrame] = false;
    _reallocateDirectionalDescriptors(currentFrame);
    _updateDirectionalTexture(currentFrame);
    updateLightDescriptors = true;
  }

  if (_changed[LightType::POINT][currentFrame]) {
    _changed[LightType::POINT][currentFrame] = false;
    _reallocatePointDescriptors(currentFrame);
    _updatePointTexture(currentFrame);
    updateLightDescriptors = true;
  }

  if (_changed[LightType::AMBIENT][currentFrame]) {
    _changed[LightType::AMBIENT][currentFrame] = false;
    _reallocateAmbientDescriptors(currentFrame);
    updateLightDescriptors = true;
  }

  // light parameters can be changed on per-frame basis
  _updateDirectionalDescriptors(currentFrame);
  _updatePointDescriptors(currentFrame);
  _updateAmbientDescriptors(currentFrame);

  if (updateLightDescriptors) _setLightDescriptors(currentFrame);
}