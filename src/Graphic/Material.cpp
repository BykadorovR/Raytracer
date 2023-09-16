#include "Material.h"

MaterialSpritePhong::MaterialSpritePhong(std::shared_ptr<CommandBuffer> commandBufferTransfer,
                                         std::shared_ptr<State> state) {
  _state = state;

  _descriptorSetLayoutCoefficients = std::make_shared<DescriptorSetLayout>(state->getDevice());
  _descriptorSetLayoutCoefficients->createUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT);

  _descriptorSetLayoutTextures = std::make_shared<DescriptorSetLayout>(state->getDevice());
  std::vector<VkDescriptorSetLayoutBinding> layoutTextures(2);
  layoutTextures[0].binding = 0;
  layoutTextures[0].descriptorCount = 1;
  layoutTextures[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutTextures[1].binding = 1;
  layoutTextures[1].descriptorCount = 1;
  layoutTextures[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  _descriptorSetLayoutTextures->createCustom(layoutTextures);

  // TODO: move to update function
  _descriptorSetTextures = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                           _descriptorSetLayoutTextures, state->getDescriptorPool(),
                                                           state->getDevice());
  _descriptorSetCoefficients = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                               _descriptorSetLayoutCoefficients,
                                                               state->getDescriptorPool(), state->getDevice());
  _uniformBufferCoefficients = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(),
                                                               sizeof(Coefficients), state->getDevice());
  _descriptorSetCoefficients->createUniformBuffer(_uniformBufferCoefficients);

  _stubTextureOne = std::make_shared<Texture>(
      "../data/Texture1x1.png", _state->getSettings()->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, 1,
      commandBufferTransfer, _state->getSettings(), _state->getDevice());
  _stubTextureZero = std::make_shared<Texture>(
      "../data/Texture1x1Black.png", _state->getSettings()->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT,
      1, commandBufferTransfer, _state->getSettings(), _state->getDevice());

  _changedTexture.resize(_state->getSettings()->getMaxFramesInFlight(), false);
  _changedCoefficients.resize(_state->getSettings()->getMaxFramesInFlight(), false);
  // initialize with empty/default data
  _textureColor = _stubTextureOne;
  _textureNormal = _stubTextureZero;
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    _updateTextureDescriptors(i);
    _updateCoefficientDescriptors(i);
  }
}

void MaterialSpritePhong::_updateCoefficientDescriptors(int currentFrame) {
  void* data;
  vkMapMemory(_state->getDevice()->getLogicalDevice(),
              _uniformBufferCoefficients->getBuffer()[currentFrame]->getMemory(), 0, sizeof(Coefficients), 0, &data);
  memcpy(data, &_coefficients, sizeof(Coefficients));
  vkUnmapMemory(_state->getDevice()->getLogicalDevice(),
                _uniformBufferCoefficients->getBuffer()[currentFrame]->getMemory());
}

void MaterialSpritePhong::_updateBaseColor(std::shared_ptr<VertexBuffer<Vertex2D>> vertexBuffer) {
  auto vertices = vertexBuffer->getData();
  for (auto& vertex : vertices) {
    vertex.color = _baseColor;
  }

  vertexBuffer->setData(vertices);
}

void MaterialSpritePhong::_updateTextureDescriptors(int currentFrame) {
  std::map<int, VkDescriptorImageInfo> images;
  VkDescriptorImageInfo colorTextureInfo{};
  colorTextureInfo.imageLayout = _textureColor->getImageView()->getImage()->getImageLayout();
  colorTextureInfo.imageView = _textureColor->getImageView()->getImageView();
  colorTextureInfo.sampler = _textureColor->getSampler()->getSampler();
  images[0] = colorTextureInfo;
  VkDescriptorImageInfo normalTextureInfo{};
  normalTextureInfo.imageLayout = _textureNormal->getImageView()->getImage()->getImageLayout();
  normalTextureInfo.imageView = _textureNormal->getImageView()->getImageView();
  normalTextureInfo.sampler = _textureNormal->getSampler()->getSampler();
  images[1] = normalTextureInfo;
  _descriptorSetTextures->createCustom(currentFrame, {}, images);
}

void MaterialSpritePhong::setBaseColor(glm::vec4 color) {
  _baseColor = color;
  _changedBaseColor = true;
}

void MaterialSpritePhong::setBaseColor(std::shared_ptr<Texture> color) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _textureColor = color;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialSpritePhong::setNormal(std::shared_ptr<Texture> normal) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _textureNormal = normal;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialSpritePhong::setPhongCoefficients(glm::vec3 ambient,
                                               glm::vec3 diffuse,
                                               glm::vec3 specular,
                                               float shininess) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _coefficients._ambient = ambient;
  _coefficients._diffuse = diffuse;
  _coefficients._specular = specular;
  _coefficients._shininess = shininess;
  for (int i = 0; i < _changedCoefficients.size(); i++) _changedCoefficients[i] = true;
}

std::shared_ptr<DescriptorSet> MaterialSpritePhong::getDescriptorSetCoefficients() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _descriptorSetCoefficients;
}

std::shared_ptr<DescriptorSetLayout> MaterialSpritePhong::getDescriptorSetLayoutCoefficients() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _descriptorSetLayoutCoefficients;
}

std::shared_ptr<DescriptorSet> MaterialSpritePhong::getDescriptorSetTextures() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _descriptorSetTextures;
}

std::shared_ptr<DescriptorSetLayout> MaterialSpritePhong::getDescriptorSetLayoutTextures() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _descriptorSetLayoutTextures;
}

void MaterialSpritePhong::draw(int currentFrame, std::shared_ptr<VertexBuffer<Vertex2D>> vertexBuffer) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  if (_changedBaseColor) {
    _updateBaseColor(vertexBuffer);
    _changedBaseColor = false;
  }
  if (_changedTexture[currentFrame]) {
    _updateTextureDescriptors(currentFrame);
    _changedTexture[currentFrame] = false;
  }
  if (_changedCoefficients[currentFrame]) {
    _updateCoefficientDescriptors(currentFrame);
    _changedCoefficients[currentFrame] = false;
  }
}