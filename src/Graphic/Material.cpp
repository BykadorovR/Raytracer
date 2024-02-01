#include "Material.h"

void Material::_updateAlphaCutoffDescriptors(int currentFrame) {
  void* data;
  vkMapMemory(_state->getDevice()->getLogicalDevice(),
              _uniformBufferAlphaCutoff->getBuffer()[currentFrame]->getMemory(), 0, sizeof(AlphaCutoff), 0, &data);
  memcpy(data, &_alphaCutoff, sizeof(AlphaCutoff));
  vkUnmapMemory(_state->getDevice()->getLogicalDevice(),
                _uniformBufferAlphaCutoff->getBuffer()[currentFrame]->getMemory());
}

Material::Material(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state) {
  _state = state;

  _descriptorSetLayoutCoefficients = std::make_shared<DescriptorSetLayout>(state->getDevice());
  _descriptorSetLayoutCoefficients->createUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT);

  _descriptorSetLayoutAlphaCutoff = std::make_shared<DescriptorSetLayout>(state->getDevice());
  _descriptorSetLayoutAlphaCutoff->createUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT);

  _descriptorSetLayoutTextures = std::make_shared<DescriptorSetLayout>(state->getDevice());

  _descriptorSetCoefficients = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                               _descriptorSetLayoutCoefficients,
                                                               state->getDescriptorPool(), state->getDevice());

  _descriptorSetAlphaCutoff = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                              _descriptorSetLayoutAlphaCutoff,
                                                              state->getDescriptorPool(), state->getDevice());
  _uniformBufferAlphaCutoff = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(),
                                                              sizeof(AlphaCutoff), state->getDevice());
  _descriptorSetAlphaCutoff->createUniformBuffer(_uniformBufferAlphaCutoff);

  _changedTexture.resize(_state->getSettings()->getMaxFramesInFlight(), false);
  _changedCoefficients.resize(_state->getSettings()->getMaxFramesInFlight(), false);
}

void Material::setDoubleSided(bool doubleSided) { _doubleSided = doubleSided; }

void Material::setAlphaCutoff(bool alphaCutoff, float alphaMask) {
  _alphaCutoff.alphaCutoff = alphaCutoff;
  _alphaCutoff.alphaMask = alphaCutoff;
}

bool Material::getDoubleSided() { return _doubleSided; }

std::shared_ptr<DescriptorSet> Material::getDescriptorSetCoefficients(int currentFrame) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  if (_changedCoefficients[currentFrame]) {
    _updateCoefficientDescriptors(currentFrame);
    _changedCoefficients[currentFrame] = false;
  }
  return _descriptorSetCoefficients;
}

std::shared_ptr<DescriptorSetLayout> Material::getDescriptorSetLayoutCoefficients() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _descriptorSetLayoutCoefficients;
}

std::shared_ptr<DescriptorSet> Material::getDescriptorSetTextures(int currentFrame) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  if (_changedTexture[currentFrame]) {
    _updateTextureDescriptors(currentFrame);
    _changedTexture[currentFrame] = false;
  }
  return _descriptorSetTextures;
}

std::shared_ptr<DescriptorSetLayout> Material::getDescriptorSetLayoutTextures() {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  return _descriptorSetLayoutTextures;
}

std::shared_ptr<DescriptorSet> Material::getDescriptorSetAlphaCutoff() { return _descriptorSetAlphaCutoff; }

std::shared_ptr<DescriptorSetLayout> Material::getDescriptorSetLayoutAlphaCutoff() {
  return _descriptorSetLayoutAlphaCutoff;
}

void MaterialPBR::_updateTextureDescriptors(int currentFrame) {
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
  VkDescriptorImageInfo metallicTextureInfo{};
  metallicTextureInfo.imageLayout = _textureMetallic->getImageView()->getImage()->getImageLayout();
  metallicTextureInfo.imageView = _textureMetallic->getImageView()->getImageView();
  metallicTextureInfo.sampler = _textureMetallic->getSampler()->getSampler();
  images[2] = metallicTextureInfo;
  VkDescriptorImageInfo roughnessTextureInfo{};
  roughnessTextureInfo.imageLayout = _textureRoughness->getImageView()->getImage()->getImageLayout();
  roughnessTextureInfo.imageView = _textureRoughness->getImageView()->getImageView();
  roughnessTextureInfo.sampler = _textureRoughness->getSampler()->getSampler();
  images[3] = roughnessTextureInfo;
  VkDescriptorImageInfo occludedTextureInfo{};
  occludedTextureInfo.imageLayout = _textureOccluded->getImageView()->getImage()->getImageLayout();
  occludedTextureInfo.imageView = _textureOccluded->getImageView()->getImageView();
  occludedTextureInfo.sampler = _textureOccluded->getSampler()->getSampler();
  images[4] = occludedTextureInfo;
  VkDescriptorImageInfo emissiveTextureInfo{};
  emissiveTextureInfo.imageLayout = _textureEmissive->getImageView()->getImage()->getImageLayout();
  emissiveTextureInfo.imageView = _textureEmissive->getImageView()->getImageView();
  emissiveTextureInfo.sampler = _textureEmissive->getSampler()->getSampler();
  images[5] = emissiveTextureInfo;
  VkDescriptorImageInfo diffuseIBLTextureInfo{};
  diffuseIBLTextureInfo.imageLayout = _textureDiffuseIBL->getImageView()->getImage()->getImageLayout();
  diffuseIBLTextureInfo.imageView = _textureDiffuseIBL->getImageView()->getImageView();
  diffuseIBLTextureInfo.sampler = _textureDiffuseIBL->getSampler()->getSampler();
  images[6] = diffuseIBLTextureInfo;
  VkDescriptorImageInfo specularIBLTextureInfo{};
  specularIBLTextureInfo.imageLayout = _textureSpecularIBL->getImageView()->getImage()->getImageLayout();
  specularIBLTextureInfo.imageView = _textureSpecularIBL->getImageView()->getImageView();
  specularIBLTextureInfo.sampler = _textureSpecularIBL->getSampler()->getSampler();
  images[7] = specularIBLTextureInfo;
  VkDescriptorImageInfo specularBRDFTextureInfo{};
  specularBRDFTextureInfo.imageLayout = _textureSpecularBRDF->getImageView()->getImage()->getImageLayout();
  specularBRDFTextureInfo.imageView = _textureSpecularBRDF->getImageView()->getImageView();
  specularBRDFTextureInfo.sampler = _textureSpecularBRDF->getSampler()->getSampler();
  images[8] = specularBRDFTextureInfo;
  _descriptorSetTextures->createCustom(currentFrame, {}, images);
}

MaterialPBR::MaterialPBR(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state)
    : Material(commandBufferTransfer, state) {
  // define textures
  std::vector<VkDescriptorSetLayoutBinding> layoutTextures(9);
  layoutTextures[0].binding = 0;
  layoutTextures[0].descriptorCount = 1;
  layoutTextures[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutTextures[1].binding = 1;
  layoutTextures[1].descriptorCount = 1;
  layoutTextures[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutTextures[2].binding = 2;
  layoutTextures[2].descriptorCount = 1;
  layoutTextures[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutTextures[3].binding = 3;
  layoutTextures[3].descriptorCount = 1;
  layoutTextures[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutTextures[4].binding = 4;
  layoutTextures[4].descriptorCount = 1;
  layoutTextures[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutTextures[5].binding = 5;
  layoutTextures[5].descriptorCount = 1;
  layoutTextures[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutTextures[6].binding = 6;
  layoutTextures[6].descriptorCount = 1;
  layoutTextures[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutTextures[7].binding = 7;
  layoutTextures[7].descriptorCount = 1;
  layoutTextures[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[7].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutTextures[8].binding = 8;
  layoutTextures[8].descriptorCount = 1;
  layoutTextures[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[8].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  _descriptorSetLayoutTextures->createCustom(layoutTextures);
  _descriptorSetTextures = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                           _descriptorSetLayoutTextures, state->getDescriptorPool(),
                                                           state->getDevice());
  // define coefficients
  _uniformBufferCoefficients = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(),
                                                               sizeof(Coefficients), state->getDevice());
  _descriptorSetCoefficients->createUniformBuffer(_uniformBufferCoefficients);

  // set default coefficients
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    _updateCoefficientDescriptors(i);
  }
}

void MaterialPBR::_updateCoefficientDescriptors(int currentFrame) {
  void* data;
  vkMapMemory(_state->getDevice()->getLogicalDevice(),
              _uniformBufferCoefficients->getBuffer()[currentFrame]->getMemory(), 0, sizeof(Coefficients), 0, &data);
  memcpy(data, &_coefficients, sizeof(Coefficients));
  vkUnmapMemory(_state->getDevice()->getLogicalDevice(),
                _uniformBufferCoefficients->getBuffer()[currentFrame]->getMemory());
}

const std::shared_ptr<Texture> MaterialPBR::getBaseColor() { return _textureColor; }

const std::shared_ptr<Texture> MaterialPBR::getNormal() { return _textureNormal; }

const std::shared_ptr<Texture> MaterialPBR::getMetallic() { return _textureMetallic; }

const std::shared_ptr<Texture> MaterialPBR::getRoughness() { return _textureRoughness; }

const std::shared_ptr<Texture> MaterialPBR::getOccluded() { return _textureOccluded; }

const std::shared_ptr<Texture> MaterialPBR::getEmissive() { return _textureEmissive; }

void MaterialPBR::setBaseColor(std::shared_ptr<Texture> color) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _textureColor = color;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPBR::setNormal(std::shared_ptr<Texture> normal) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _textureNormal = normal;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPBR::setMetallic(std::shared_ptr<Texture> metallic) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _textureMetallic = metallic;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPBR::setRoughness(std::shared_ptr<Texture> roughness) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _textureRoughness = roughness;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPBR::setOccluded(std::shared_ptr<Texture> occluded) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _textureOccluded = occluded;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPBR::setDiffuseIBL(std::shared_ptr<Texture> diffuseIBL) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _textureDiffuseIBL = diffuseIBL;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPBR::setSpecularIBL(std::shared_ptr<Texture> specularIBL, std::shared_ptr<Texture> specularBRDF) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _textureSpecularIBL = specularIBL;
  _textureSpecularBRDF = specularBRDF;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPBR::setEmissive(std::shared_ptr<Texture> emissive) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _textureEmissive = emissive;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPBR::setCoefficients(float metallicFactor,
                                  float roughnessFactor,
                                  float occlusionStrength,
                                  glm::vec3 emissiveFactor) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _coefficients.metallicFactor = metallicFactor;
  _coefficients.roughnessFactor = roughnessFactor;
  _coefficients.occlusionStrength = occlusionStrength;
  _coefficients.emissiveFactor = emissiveFactor;
  for (int i = 0; i < _changedCoefficients.size(); i++) _changedCoefficients[i] = true;
}

MaterialPhong::MaterialPhong(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state)
    : Material(commandBufferTransfer, state) {
  std::vector<VkDescriptorSetLayoutBinding> layoutTextures(3);
  layoutTextures[0].binding = 0;
  layoutTextures[0].descriptorCount = 1;
  layoutTextures[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutTextures[1].binding = 1;
  layoutTextures[1].descriptorCount = 1;
  layoutTextures[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutTextures[2].binding = 2;
  layoutTextures[2].descriptorCount = 1;
  layoutTextures[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  _descriptorSetLayoutTextures->createCustom(layoutTextures);

  _descriptorSetTextures = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                           _descriptorSetLayoutTextures, state->getDescriptorPool(),
                                                           state->getDevice());

  _uniformBufferCoefficients = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(),
                                                               sizeof(Coefficients), state->getDevice());
  _descriptorSetCoefficients->createUniformBuffer(_uniformBufferCoefficients);
  // set default coefficients
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    _updateCoefficientDescriptors(i);
  }
}

void MaterialPhong::_updateCoefficientDescriptors(int currentFrame) {
  void* data;
  vkMapMemory(_state->getDevice()->getLogicalDevice(),
              _uniformBufferCoefficients->getBuffer()[currentFrame]->getMemory(), 0, sizeof(Coefficients), 0, &data);
  memcpy(data, &_coefficients, sizeof(Coefficients));
  vkUnmapMemory(_state->getDevice()->getLogicalDevice(),
                _uniformBufferCoefficients->getBuffer()[currentFrame]->getMemory());
}

void MaterialPhong::_updateTextureDescriptors(int currentFrame) {
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
  VkDescriptorImageInfo specularTextureInfo{};
  specularTextureInfo.imageLayout = _textureSpecular->getImageView()->getImage()->getImageLayout();
  specularTextureInfo.imageView = _textureSpecular->getImageView()->getImageView();
  specularTextureInfo.sampler = _textureSpecular->getSampler()->getSampler();
  images[2] = specularTextureInfo;
  _descriptorSetTextures->createCustom(currentFrame, {}, images);
}

const std::shared_ptr<Texture> MaterialPhong::getBaseColor() { return _textureColor; }

const std::shared_ptr<Texture> MaterialPhong::getNormal() { return _textureNormal; }

const std::shared_ptr<Texture> MaterialPhong::getSpecular() { return _textureSpecular; }

void MaterialPhong::setBaseColor(std::shared_ptr<Texture> color) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _textureColor = color;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPhong::setSpecular(std::shared_ptr<Texture> specular) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _textureSpecular = specular;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPhong::setNormal(std::shared_ptr<Texture> normal) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _textureNormal = normal;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPhong::setCoefficients(glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, float shininess) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _coefficients._ambient = ambient;
  _coefficients._diffuse = diffuse;
  _coefficients._specular = specular;
  _coefficients._shininess = shininess;
  for (int i = 0; i < _changedCoefficients.size(); i++) _changedCoefficients[i] = true;
}

MaterialColor::MaterialColor(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state)
    : Material(commandBufferTransfer, state) {
  std::vector<VkDescriptorSetLayoutBinding> layoutTextures(1);
  layoutTextures[0].binding = 0;
  layoutTextures[0].descriptorCount = 1;
  layoutTextures[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  _descriptorSetLayoutTextures->createCustom(layoutTextures);

  _descriptorSetTextures = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                           _descriptorSetLayoutTextures, state->getDescriptorPool(),
                                                           state->getDevice());
}

void MaterialColor::_updateCoefficientDescriptors(int currentFrame) {}

void MaterialColor::_updateTextureDescriptors(int currentFrame) {
  std::map<int, VkDescriptorImageInfo> images;
  VkDescriptorImageInfo colorTextureInfo{};
  colorTextureInfo.imageLayout = _textureColor->getImageView()->getImage()->getImageLayout();
  colorTextureInfo.imageView = _textureColor->getImageView()->getImageView();
  colorTextureInfo.sampler = _textureColor->getSampler()->getSampler();
  images[0] = colorTextureInfo;
  _descriptorSetTextures->createCustom(currentFrame, {}, images);
}

std::shared_ptr<Texture> MaterialColor::getBaseColor() { return _textureColor; }

void MaterialColor::setBaseColor(std::shared_ptr<Texture> color) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _textureColor = color;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}