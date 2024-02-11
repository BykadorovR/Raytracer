#include "LightManager.h"
#include "Buffer.h"

LightManager::LightManager(std::shared_ptr<CommandBuffer> commandBufferTransfer,
                           std::shared_ptr<ResourceManager> resourceManager,
                           std::shared_ptr<State> state) {
  _state = state;
  _commandBufferTransfer = commandBufferTransfer;

  _descriptorPool = std::make_shared<DescriptorPool>(_descriptorPoolSize, _state->getDevice());

  // create descriptor layouts for light info for direct, point and ambient lights for fragment shaders
  {
    _descriptorSetLayoutLightPBR = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutLightPBR(2);
    layoutLightPBR[0].binding = 0;
    layoutLightPBR[0].descriptorCount = 1;
    layoutLightPBR[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutLightPBR[0].pImmutableSamplers = nullptr;
    layoutLightPBR[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutLightPBR[1].binding = 1;
    layoutLightPBR[1].descriptorCount = 1;
    layoutLightPBR[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutLightPBR[1].pImmutableSamplers = nullptr;
    layoutLightPBR[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    _descriptorSetLayoutLightPBR->createCustom(layoutLightPBR);

    _descriptorSetLayoutLightPhong = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutLightPhong(3);
    layoutLightPhong[0].binding = 0;
    layoutLightPhong[0].descriptorCount = 1;
    layoutLightPhong[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutLightPhong[0].pImmutableSamplers = nullptr;
    layoutLightPhong[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutLightPhong[1].binding = 1;
    layoutLightPhong[1].descriptorCount = 1;
    layoutLightPhong[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutLightPhong[1].pImmutableSamplers = nullptr;
    layoutLightPhong[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutLightPhong[2].binding = 2;
    layoutLightPhong[2].descriptorCount = 1;
    layoutLightPhong[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutLightPhong[2].pImmutableSamplers = nullptr;
    layoutLightPhong[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    _descriptorSetLayoutLightPhong->createCustom(layoutLightPhong);
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

  _descriptorSetLightPhong = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                             _descriptorSetLayoutLightPhong, _descriptorPool,
                                                             _state->getDevice());
  _descriptorSetLightPBR = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                           _descriptorSetLayoutLightPBR, _descriptorPool,
                                                           _state->getDevice());

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
  _stubTexture = std::make_shared<Texture>(resourceManager->loadImage({"assets/stubs/Texture1x1.png"}),
                                           _state->getSettings()->getLoadTextureColorFormat(),
                                           VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer, _state);
  _stubCubemap = std::make_shared<Cubemap>(
      resourceManager->loadImage(std::vector<std::string>(6, "assets/stubs/Texture1x1.png")),
      _state->getSettings()->getLoadTextureColorFormat(), 1, VK_IMAGE_ASPECT_COLOR_BIT,
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
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoPBR;
    std::vector<VkDescriptorBufferInfo> bufferDirectionalInfo(1);
    bufferDirectionalInfo[0].buffer = VK_NULL_HANDLE;
    bufferDirectionalInfo[0].offset = 0;
    bufferDirectionalInfo[0].range = VK_WHOLE_SIZE;
    if (_lightDirectionalSSBO.size() > currentFrame && _lightDirectionalSSBO[currentFrame]) {
      bufferDirectionalInfo[0].buffer = _lightDirectionalSSBO[currentFrame]->getData();
      bufferDirectionalInfo[0].range = _lightDirectionalSSBO[currentFrame]->getSize();
    }
    bufferInfoPBR[0] = bufferDirectionalInfo;

    std::vector<VkDescriptorBufferInfo> bufferPointInfo(1);
    bufferPointInfo[0].buffer = VK_NULL_HANDLE;
    bufferPointInfo[0].offset = 0;
    bufferPointInfo[0].range = VK_WHOLE_SIZE;
    if (_lightPointSSBO.size() > currentFrame && _lightPointSSBO[currentFrame]) {
      bufferPointInfo[0].buffer = _lightPointSSBO[currentFrame]->getData();
      bufferPointInfo[0].range = _lightPointSSBO[currentFrame]->getSize();
    }
    bufferInfoPBR[1] = bufferPointInfo;
    _descriptorSetLightPBR->createCustom(currentFrame, bufferInfoPBR, {});

    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoPhong;
    bufferInfoPhong[0] = bufferDirectionalInfo;
    bufferInfoPhong[1] = bufferPointInfo;

    std::vector<VkDescriptorBufferInfo> bufferAmbientInfo(1);
    bufferAmbientInfo[0].buffer = VK_NULL_HANDLE;
    bufferAmbientInfo[0].offset = 0;
    bufferAmbientInfo[0].range = VK_WHOLE_SIZE;
    if (_lightAmbientSSBO.size() > currentFrame && _lightAmbientSSBO[currentFrame]) {
      bufferAmbientInfo[0].buffer = _lightAmbientSSBO[currentFrame]->getData();
      bufferAmbientInfo[0].range = _lightAmbientSSBO[currentFrame]->getSize();
    }
    bufferInfoPhong[2] = bufferAmbientInfo;
    _descriptorSetLightPhong->createCustom(currentFrame, bufferInfoPhong, {});
  }

  // light VP matrices for models/sprites
  {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfo;
    std::vector<VkDescriptorBufferInfo> bufferDirectionalInfo(1);
    bufferDirectionalInfo[0].buffer = VK_NULL_HANDLE;
    bufferDirectionalInfo[0].offset = 0;
    bufferDirectionalInfo[0].range = VK_WHOLE_SIZE;
    if (_lightDirectionalSSBOViewProjection.size() > currentFrame &&
        _lightDirectionalSSBOViewProjection[currentFrame]) {
      bufferDirectionalInfo[0].buffer = _lightDirectionalSSBOViewProjection[currentFrame]->getData();
      bufferDirectionalInfo[0].range = _lightDirectionalSSBOViewProjection[currentFrame]->getSize();
    }
    bufferInfo[0] = bufferDirectionalInfo;

    std::vector<VkDescriptorBufferInfo> bufferPointInfo(1);
    bufferPointInfo[0].buffer = VK_NULL_HANDLE;
    bufferPointInfo[0].offset = 0;
    bufferPointInfo[0].range = VK_WHOLE_SIZE;
    if (_lightPointSSBOViewProjection.size() > currentFrame && _lightPointSSBOViewProjection[currentFrame]) {
      bufferPointInfo[0].buffer = _lightPointSSBOViewProjection[currentFrame]->getData();
      bufferPointInfo[0].range = _lightPointSSBOViewProjection[currentFrame]->getSize();
    }
    bufferInfo[1] = bufferPointInfo;

    _descriptorSetViewProjection[VK_SHADER_STAGE_VERTEX_BIT]->createCustom(currentFrame, bufferInfo, {});
  }

  // light VP matrices for terrain
  {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfo;
    std::vector<VkDescriptorBufferInfo> bufferDirectionalInfo(1);
    bufferDirectionalInfo[0].buffer = VK_NULL_HANDLE;
    bufferDirectionalInfo[0].offset = 0;
    bufferDirectionalInfo[0].range = VK_WHOLE_SIZE;
    if (_lightDirectionalSSBOViewProjection.size() > currentFrame &&
        _lightDirectionalSSBOViewProjection[currentFrame]) {
      bufferDirectionalInfo[0].buffer = _lightDirectionalSSBOViewProjection[currentFrame]->getData();
      bufferDirectionalInfo[0].range = _lightDirectionalSSBOViewProjection[currentFrame]->getSize();
    }
    bufferInfo[0] = bufferDirectionalInfo;

    std::vector<VkDescriptorBufferInfo> bufferPointInfo(1);
    bufferPointInfo[0].buffer = VK_NULL_HANDLE;
    bufferPointInfo[0].offset = 0;
    bufferPointInfo[0].range = VK_WHOLE_SIZE;
    if (_lightPointSSBOViewProjection.size() > currentFrame && _lightPointSSBOViewProjection[currentFrame]) {
      bufferPointInfo[0].buffer = _lightPointSSBOViewProjection[currentFrame]->getData();
      bufferPointInfo[0].range = _lightPointSSBOViewProjection[currentFrame]->getSize();
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
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);
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
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);
  }

  size = 0;
  for (int i = 0; i < _directionalLights.size(); i++) {
    size += sizeof(glm::mat4);
  }

  if (_directionalLights.size() > 0) {
    _lightDirectionalSSBOViewProjection[currentFrame] = std::make_shared<Buffer>(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);
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
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);
  }

  size = 0;
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

std::shared_ptr<DescriptorSetLayout> LightManager::getDSLViewProjection(VkShaderStageFlagBits stage) {
  return _descriptorSetLayoutViewProjection[stage];
}

std::shared_ptr<DescriptorSet> LightManager::getDSViewProjection(VkShaderStageFlagBits stage) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _descriptorSetViewProjection[stage];
}

std::shared_ptr<DescriptorSetLayout> LightManager::getDSLLightPhong() { return _descriptorSetLayoutLightPhong; }
std::shared_ptr<DescriptorSetLayout> LightManager::getDSLLightPBR() { return _descriptorSetLayoutLightPBR; }

std::shared_ptr<DescriptorSet> LightManager::getDSLightPhong() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _descriptorSetLightPhong;
}

std::shared_ptr<DescriptorSet> LightManager::getDSLightPBR() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _descriptorSetLightPBR;
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