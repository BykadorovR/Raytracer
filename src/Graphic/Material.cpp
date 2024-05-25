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
                                                              sizeof(AlphaCutoff), state);
  _descriptorSetAlphaCutoff->createUniformBuffer(_uniformBufferAlphaCutoff);

  _changedTexture.resize(_state->getSettings()->getMaxFramesInFlight(), false);
  _changedCoefficients.resize(_state->getSettings()->getMaxFramesInFlight(), false);
}

void Material::setDoubleSided(bool doubleSided) { _doubleSided = doubleSided; }

void Material::setAlphaCutoff(bool alphaCutoff, float alphaMask) {
  _alphaCutoff.alphaCutoff = alphaCutoff;
  _alphaCutoff.alphaMask = alphaCutoff;
  for (int i = 0; i < _state->getFrameInFlight(); i++) _updateAlphaCutoffDescriptors(i);
}

bool Material::getDoubleSided() { return _doubleSided; }

std::shared_ptr<UniformBuffer> Material::getBufferCoefficients() { return _uniformBufferCoefficients; }

std::shared_ptr<UniformBuffer> Material::getBufferAlphaCutoff() { return _uniformBufferAlphaCutoff; }

std::shared_ptr<DescriptorSet> Material::getDescriptorSetCoefficients(int currentFrame) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
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
  std::map<int, std::vector<VkDescriptorImageInfo>> images;
  std::vector<VkDescriptorImageInfo> colorTextureInfo(_textureColor.size());
  for (int i = 0; i < _textureColor.size(); i++) {
    colorTextureInfo[i].imageLayout = _textureColor[i]->getImageView()->getImage()->getImageLayout();
    colorTextureInfo[i].imageView = _textureColor[i]->getImageView()->getImageView();
    colorTextureInfo[i].sampler = _textureColor[i]->getSampler()->getSampler();
  }
  images[0] = colorTextureInfo;
  std::vector<VkDescriptorImageInfo> normalTextureInfo(_textureNormal.size());
  for (int i = 0; i < _textureColor.size(); i++) {
    normalTextureInfo[i].imageLayout = _textureNormal[i]->getImageView()->getImage()->getImageLayout();
    normalTextureInfo[i].imageView = _textureNormal[i]->getImageView()->getImageView();
    normalTextureInfo[i].sampler = _textureNormal[i]->getSampler()->getSampler();
  }
  images[1] = normalTextureInfo;
  std::vector<VkDescriptorImageInfo> metallicTextureInfo(_textureMetallic.size());
  for (int i = 0; i < _textureColor.size(); i++) {
    metallicTextureInfo[i].imageLayout = _textureMetallic[i]->getImageView()->getImage()->getImageLayout();
    metallicTextureInfo[i].imageView = _textureMetallic[i]->getImageView()->getImageView();
    metallicTextureInfo[i].sampler = _textureMetallic[i]->getSampler()->getSampler();
  }
  images[2] = metallicTextureInfo;
  std::vector<VkDescriptorImageInfo> roughnessTextureInfo(_textureRoughness.size());
  for (int i = 0; i < _textureColor.size(); i++) {
    roughnessTextureInfo[i].imageLayout = _textureRoughness[i]->getImageView()->getImage()->getImageLayout();
    roughnessTextureInfo[i].imageView = _textureRoughness[i]->getImageView()->getImageView();
    roughnessTextureInfo[i].sampler = _textureRoughness[i]->getSampler()->getSampler();
  }
  images[3] = roughnessTextureInfo;
  std::vector<VkDescriptorImageInfo> occludedTextureInfo(_textureOccluded.size());
  for (int i = 0; i < _textureColor.size(); i++) {
    occludedTextureInfo[i].imageLayout = _textureOccluded[i]->getImageView()->getImage()->getImageLayout();
    occludedTextureInfo[i].imageView = _textureOccluded[i]->getImageView()->getImageView();
    occludedTextureInfo[i].sampler = _textureOccluded[i]->getSampler()->getSampler();
  }
  images[4] = occludedTextureInfo;
  std::vector<VkDescriptorImageInfo> emissiveTextureInfo(_textureEmissive.size());
  for (int i = 0; i < _textureColor.size(); i++) {
    emissiveTextureInfo[i].imageLayout = _textureEmissive[i]->getImageView()->getImage()->getImageLayout();
    emissiveTextureInfo[i].imageView = _textureEmissive[i]->getImageView()->getImageView();
    emissiveTextureInfo[i].sampler = _textureEmissive[i]->getSampler()->getSampler();
  }
  images[5] = emissiveTextureInfo;
  VkDescriptorImageInfo diffuseIBLTextureInfo{};
  diffuseIBLTextureInfo.imageLayout = _textureDiffuseIBL->getImageView()->getImage()->getImageLayout();
  diffuseIBLTextureInfo.imageView = _textureDiffuseIBL->getImageView()->getImageView();
  diffuseIBLTextureInfo.sampler = _textureDiffuseIBL->getSampler()->getSampler();
  images[6] = {diffuseIBLTextureInfo};
  VkDescriptorImageInfo specularIBLTextureInfo{};
  specularIBLTextureInfo.imageLayout = _textureSpecularIBL->getImageView()->getImage()->getImageLayout();
  specularIBLTextureInfo.imageView = _textureSpecularIBL->getImageView()->getImageView();
  specularIBLTextureInfo.sampler = _textureSpecularIBL->getSampler()->getSampler();
  images[7] = {specularIBLTextureInfo};
  VkDescriptorImageInfo specularBRDFTextureInfo{};
  specularBRDFTextureInfo.imageLayout = _textureSpecularBRDF->getImageView()->getImage()->getImageLayout();
  specularBRDFTextureInfo.imageView = _textureSpecularBRDF->getImageView()->getImageView();
  specularBRDFTextureInfo.sampler = _textureSpecularBRDF->getSampler()->getSampler();
  images[8] = {specularBRDFTextureInfo};
  _descriptorSetTextures->createCustom(currentFrame, {}, images);
}

MaterialPBR::MaterialPBR(MaterialTarget target,
                         std::shared_ptr<CommandBuffer> commandBufferTransfer,
                         std::shared_ptr<State> state)
    : Material(commandBufferTransfer, state) {
  _target = target;
  // define textures
  std::vector<VkDescriptorSetLayoutBinding> layoutTextures(9);
  layoutTextures[0].binding = 0;
  layoutTextures[0].descriptorCount = static_cast<int>(target);
  layoutTextures[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutTextures[1].binding = 1;
  layoutTextures[1].descriptorCount = static_cast<int>(target);
  layoutTextures[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutTextures[2].binding = 2;
  layoutTextures[2].descriptorCount = static_cast<int>(target);
  layoutTextures[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutTextures[3].binding = 3;
  layoutTextures[3].descriptorCount = static_cast<int>(target);
  layoutTextures[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutTextures[4].binding = 4;
  layoutTextures[4].descriptorCount = static_cast<int>(target);
  layoutTextures[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutTextures[5].binding = 5;
  layoutTextures[5].descriptorCount = static_cast<int>(target);
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
                                                               sizeof(Coefficients), state);
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

const std::vector<std::shared_ptr<Texture>> MaterialPBR::getBaseColor() { return _textureColor; }

const std::vector<std::shared_ptr<Texture>> MaterialPBR::getNormal() { return _textureNormal; }

const std::vector<std::shared_ptr<Texture>> MaterialPBR::getMetallic() { return _textureMetallic; }

const std::vector<std::shared_ptr<Texture>> MaterialPBR::getRoughness() { return _textureRoughness; }

const std::vector<std::shared_ptr<Texture>> MaterialPBR::getOccluded() { return _textureOccluded; }

const std::vector<std::shared_ptr<Texture>> MaterialPBR::getEmissive() { return _textureEmissive; }

std::shared_ptr<Texture> MaterialPBR::getDiffuseIBL() { return _textureDiffuseIBL; }

std::shared_ptr<Texture> MaterialPBR::getSpecularIBL() { return _textureSpecularIBL; }

std::shared_ptr<Texture> MaterialPBR::getSpecularBRDF() { return _textureSpecularBRDF; }

void MaterialPBR::setBaseColor(std::vector<std::shared_ptr<Texture>> color) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  if (static_cast<int>(_target) != color.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textureColor = color;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPBR::setNormal(std::vector<std::shared_ptr<Texture>> normal) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  if (static_cast<int>(_target) != normal.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textureNormal = normal;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPBR::setMetallic(std::vector<std::shared_ptr<Texture>> metallic) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  if (static_cast<int>(_target) != metallic.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textureMetallic = metallic;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPBR::setRoughness(std::vector<std::shared_ptr<Texture>> roughness) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  if (static_cast<int>(_target) != roughness.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textureRoughness = roughness;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPBR::setOccluded(std::vector<std::shared_ptr<Texture>> occluded) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  if (static_cast<int>(_target) != occluded.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textureOccluded = occluded;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPBR::setEmissive(std::vector<std::shared_ptr<Texture>> emissive) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  if (static_cast<int>(_target) != emissive.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textureEmissive = emissive;
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

void MaterialPBR::setCoefficients(float metallicFactor,
                                  float roughnessFactor,
                                  float occlusionStrength,
                                  glm::vec3 emissiveFactor) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _coefficients.metallicFactor = metallicFactor;
  _coefficients.roughnessFactor = roughnessFactor;
  _coefficients.occlusionStrength = occlusionStrength;
  _coefficients.emissiveFactor = emissiveFactor;
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) _updateCoefficientDescriptors(i);
}

MaterialPhong::MaterialPhong(MaterialTarget target,
                             std::shared_ptr<CommandBuffer> commandBufferTransfer,
                             std::shared_ptr<State> state)
    : Material(commandBufferTransfer, state) {
  _target = target;

  std::vector<VkDescriptorSetLayoutBinding> layoutTextures(3);
  layoutTextures[0].binding = 0;
  layoutTextures[0].descriptorCount = static_cast<int>(target);
  layoutTextures[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutTextures[1].binding = 1;
  layoutTextures[1].descriptorCount = static_cast<int>(target);
  layoutTextures[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutTextures[2].binding = 2;
  layoutTextures[2].descriptorCount = static_cast<int>(target);
  layoutTextures[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  _descriptorSetLayoutTextures->createCustom(layoutTextures);

  _descriptorSetTextures = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                           _descriptorSetLayoutTextures, state->getDescriptorPool(),
                                                           state->getDevice());

  _uniformBufferCoefficients = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(),
                                                               sizeof(Coefficients), state);
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
  std::map<int, std::vector<VkDescriptorImageInfo>> images;
  std::vector<VkDescriptorImageInfo> colorTextureInfo(_textureColor.size());
  for (int i = 0; i < _textureColor.size(); i++) {
    colorTextureInfo[i].imageLayout = _textureColor[i]->getImageView()->getImage()->getImageLayout();
    colorTextureInfo[i].imageView = _textureColor[i]->getImageView()->getImageView();
    colorTextureInfo[i].sampler = _textureColor[i]->getSampler()->getSampler();
  }
  images[0] = colorTextureInfo;
  std::vector<VkDescriptorImageInfo> normalTextureInfo(_textureNormal.size());
  for (int i = 0; i < _textureColor.size(); i++) {
    normalTextureInfo[i].imageLayout = _textureNormal[i]->getImageView()->getImage()->getImageLayout();
    normalTextureInfo[i].imageView = _textureNormal[i]->getImageView()->getImageView();
    normalTextureInfo[i].sampler = _textureNormal[i]->getSampler()->getSampler();
  }
  images[1] = normalTextureInfo;
  std::vector<VkDescriptorImageInfo> specularTextureInfo(_textureSpecular.size());
  for (int i = 0; i < _textureColor.size(); i++) {
    specularTextureInfo[i].imageLayout = _textureSpecular[i]->getImageView()->getImage()->getImageLayout();
    specularTextureInfo[i].imageView = _textureSpecular[i]->getImageView()->getImageView();
    specularTextureInfo[i].sampler = _textureSpecular[i]->getSampler()->getSampler();
  }
  images[2] = specularTextureInfo;
  _descriptorSetTextures->createCustom(currentFrame, {}, images);
}

const std::vector<std::shared_ptr<Texture>> MaterialPhong::getBaseColor() { return _textureColor; }

const std::vector<std::shared_ptr<Texture>> MaterialPhong::getNormal() { return _textureNormal; }

const std::vector<std::shared_ptr<Texture>> MaterialPhong::getSpecular() { return _textureSpecular; }

void MaterialPhong::setBaseColor(std::vector<std::shared_ptr<Texture>> color) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  if (static_cast<int>(_target) != color.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textureColor = color;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPhong::setSpecular(std::vector<std::shared_ptr<Texture>> specular) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  if (static_cast<int>(_target) != specular.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textureSpecular = specular;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPhong::setNormal(std::vector<std::shared_ptr<Texture>> normal) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  if (static_cast<int>(_target) != normal.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textureNormal = normal;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}

void MaterialPhong::setCoefficients(glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, float shininess) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  _coefficients._ambient = ambient;
  _coefficients._diffuse = diffuse;
  _coefficients._specular = specular;
  _coefficients._shininess = shininess;
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) _updateCoefficientDescriptors(i);
}

MaterialColor::MaterialColor(MaterialTarget target,
                             std::shared_ptr<CommandBuffer> commandBufferTransfer,
                             std::shared_ptr<State> state)
    : Material(commandBufferTransfer, state) {
  _target = target;

  std::vector<VkDescriptorSetLayoutBinding> layoutTextures(1);
  layoutTextures[0].binding = 0;
  layoutTextures[0].descriptorCount = static_cast<int>(target);
  layoutTextures[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutTextures[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  _descriptorSetLayoutTextures->createCustom(layoutTextures);

  _descriptorSetTextures = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                           _descriptorSetLayoutTextures, state->getDescriptorPool(),
                                                           state->getDevice());
}

void MaterialColor::_updateCoefficientDescriptors(int currentFrame) {}

void MaterialColor::_updateTextureDescriptors(int currentFrame) {
  std::map<int, std::vector<VkDescriptorImageInfo>> images;
  std::vector<VkDescriptorImageInfo> colorTextureInfo(_textureColor.size());
  for (int i = 0; i < _textureColor.size(); i++) {
    colorTextureInfo[i].imageLayout = _textureColor[i]->getImageView()->getImage()->getImageLayout();
    colorTextureInfo[i].imageView = _textureColor[i]->getImageView()->getImageView();
    colorTextureInfo[i].sampler = _textureColor[i]->getSampler()->getSampler();
  }
  images[0] = colorTextureInfo;
  _descriptorSetTextures->createCustom(currentFrame, {}, images);
}

const std::vector<std::shared_ptr<Texture>> MaterialColor::getBaseColor() { return _textureColor; }

void MaterialColor::setBaseColor(std::vector<std::shared_ptr<Texture>> color) {
  std::unique_lock<std::mutex> accessLock(_accessMutex);
  if (static_cast<int>(_target) != color.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textureColor = color;
  for (int i = 0; i < _changedTexture.size(); i++) _changedTexture[i] = true;
}