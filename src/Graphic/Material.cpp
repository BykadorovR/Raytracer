#include "Graphic/Material.h"

void Material::_updateAlphaCutoffDescriptors(int currentFrame) {
  _uniformBufferAlphaCutoff[currentFrame]->setData(&_alphaCutoff);
}

void Material::_updateDescriptor(int frameInFlight, MaterialTexture type) {
  for (auto& [descriptor, info] : _descriptorsUpdate) {
    if (info._frameInFlight == frameInFlight) {
      for (auto& [material, slot] : info._materialInfo) {
        if (material == type) {
          std::map<int, std::vector<VkDescriptorImageInfo>> images;
          std::vector<VkDescriptorImageInfo> colorTextureInfo(_textures[type].size());
          for (int i = 0; i < _textures[type].size(); i++) {
            colorTextureInfo[i] = {.sampler = _textures[type][i]->getSampler()->getSampler(),
                                   .imageView = _textures[type][i]->getImageView()->getImageView(),
                                   .imageLayout = _textures[type][i]->getImageView()->getImage()->getImageLayout()};
          }
          images[slot] = colorTextureInfo;
          descriptor->createCustom({}, images);
        }
      }
    }
  }
}

void Material::update(int currentFrame) {
  std::unique_lock<std::mutex> lock(_mutex);
  for (auto& [material, status] : _changedTextures) {
    if (status[currentFrame]) {
      _updateDescriptor(currentFrame, material);
      status[currentFrame] = false;
    }
  }
  if (_changedAlpha[currentFrame]) {
    _updateAlphaCutoffDescriptors(currentFrame);
    _changedAlpha[currentFrame] = false;
  }
  if (_changedCoefficients[currentFrame]) {
    _updateCoefficientBuffer(currentFrame);
    _changedCoefficients[currentFrame] = false;
  }
}

Material::Material(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<EngineState> engineState) {
  _engineState = engineState;

  _uniformBufferAlphaCutoff.resize(_engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++)
    _uniformBufferAlphaCutoff[i] = std::make_shared<Buffer>(
        sizeof(AlphaCutoff), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, engineState);
  _changedAlpha.resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
}

void Material::unregisterUpdate(std::shared_ptr<DescriptorSet> descriptor) { _descriptorsUpdate.erase(descriptor); }

void Material::registerUpdate(std::shared_ptr<DescriptorSet> descriptor, MaterialInfoUpdate info) {
  _descriptorsUpdate[descriptor] = info;
}

void Material::setDoubleSided(bool doubleSided) { _doubleSided = doubleSided; }

void Material::setAlphaCutoff(bool alphaMask, float alphaCutoff) {
  std::unique_lock<std::mutex> lock(_mutex);
  _alphaCutoff = {.alphaMask = alphaMask, .alphaCutoff = alphaCutoff};
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) _changedAlpha[i] = true;
}

bool Material::getDoubleSided() { return _doubleSided; }

std::vector<std::shared_ptr<Buffer>> Material::getBufferCoefficients() { return _uniformBufferCoefficients; }

std::vector<std::shared_ptr<Buffer>> Material::getBufferAlphaCutoff() { return _uniformBufferAlphaCutoff; }

MaterialPBR::MaterialPBR(MaterialTarget target,
                         std::shared_ptr<CommandBuffer> commandBufferTransfer,
                         std::shared_ptr<EngineState> engineState)
    : Material(commandBufferTransfer, engineState) {
  _target = target;
  // define coefficients
  _uniformBufferCoefficients.resize(_engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++)
    _uniformBufferCoefficients[i] = std::make_shared<Buffer>(
        sizeof(Coefficients), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, engineState);
  _changedTextures[MaterialTexture::COLOR].resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
  _changedTextures[MaterialTexture::NORMAL].resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
  _changedTextures[MaterialTexture::METALLIC].resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
  _changedTextures[MaterialTexture::ROUGHNESS].resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
  _changedTextures[MaterialTexture::OCCLUSION].resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
  _changedTextures[MaterialTexture::EMISSIVE].resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
  _changedTextures[MaterialTexture::IBL_DIFFUSE].resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
  _changedTextures[MaterialTexture::IBL_SPECULAR].resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
  _changedTextures[MaterialTexture::BRDF_SPECULAR].resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
  _changedCoefficients.resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
  // set default coefficients
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _updateCoefficientBuffer(i);
  }
}

void MaterialPBR::_updateCoefficientBuffer(int currentFrame) {
  _uniformBufferCoefficients[currentFrame]->setData(&_coefficients);
}

const std::vector<std::shared_ptr<Texture>> MaterialPBR::getBaseColor() { return _textures[MaterialTexture::COLOR]; }

const std::vector<std::shared_ptr<Texture>> MaterialPBR::getNormal() { return _textures[MaterialTexture::NORMAL]; }

const std::vector<std::shared_ptr<Texture>> MaterialPBR::getMetallic() { return _textures[MaterialTexture::METALLIC]; }

const std::vector<std::shared_ptr<Texture>> MaterialPBR::getRoughness() {
  return _textures[MaterialTexture::ROUGHNESS];
}

const std::vector<std::shared_ptr<Texture>> MaterialPBR::getOccluded() { return _textures[MaterialTexture::OCCLUSION]; }

const std::vector<std::shared_ptr<Texture>> MaterialPBR::getEmissive() { return _textures[MaterialTexture::EMISSIVE]; }

std::shared_ptr<Texture> MaterialPBR::getDiffuseIBL() { return _textures[MaterialTexture::IBL_DIFFUSE][0]; }

std::shared_ptr<Texture> MaterialPBR::getSpecularIBL() { return _textures[MaterialTexture::IBL_SPECULAR][0]; }

std::shared_ptr<Texture> MaterialPBR::getSpecularBRDF() { return _textures[MaterialTexture::BRDF_SPECULAR][0]; }

void MaterialPBR::setBaseColor(std::vector<std::shared_ptr<Texture>> color) {
  std::unique_lock<std::mutex> lock(_mutex);
  if (static_cast<int>(_target) != color.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textures[MaterialTexture::COLOR] = color;
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _changedTextures[MaterialTexture::COLOR][i] = true;
  }
}

void MaterialPBR::setNormal(std::vector<std::shared_ptr<Texture>> normal) {
  std::unique_lock<std::mutex> lock(_mutex);
  if (static_cast<int>(_target) != normal.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textures[MaterialTexture::NORMAL] = normal;
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _changedTextures[MaterialTexture::NORMAL][i] = true;
  }
}

void MaterialPBR::setMetallic(std::vector<std::shared_ptr<Texture>> metallic) {
  std::unique_lock<std::mutex> lock(_mutex);
  if (static_cast<int>(_target) != metallic.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textures[MaterialTexture::METALLIC] = metallic;
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _changedTextures[MaterialTexture::METALLIC][i] = true;
  }
}

void MaterialPBR::setRoughness(std::vector<std::shared_ptr<Texture>> roughness) {
  std::unique_lock<std::mutex> lock(_mutex);
  if (static_cast<int>(_target) != roughness.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textures[MaterialTexture::ROUGHNESS] = roughness;
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _changedTextures[MaterialTexture::ROUGHNESS][i] = true;
  }
}

void MaterialPBR::setOccluded(std::vector<std::shared_ptr<Texture>> occluded) {
  std::unique_lock<std::mutex> lock(_mutex);
  if (static_cast<int>(_target) != occluded.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textures[MaterialTexture::OCCLUSION] = occluded;
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _changedTextures[MaterialTexture::OCCLUSION][i] = true;
  }
}

void MaterialPBR::setEmissive(std::vector<std::shared_ptr<Texture>> emissive) {
  std::unique_lock<std::mutex> lock(_mutex);
  if (static_cast<int>(_target) != emissive.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textures[MaterialTexture::EMISSIVE] = emissive;
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _changedTextures[MaterialTexture::EMISSIVE][i] = true;
  }
}

void MaterialPBR::setDiffuseIBL(std::shared_ptr<Texture> diffuseIBL) {
  std::unique_lock<std::mutex> lock(_mutex);
  _textures[MaterialTexture::IBL_DIFFUSE] = {diffuseIBL};
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _changedTextures[MaterialTexture::IBL_DIFFUSE][i] = true;
  }
}

void MaterialPBR::setSpecularIBL(std::shared_ptr<Texture> specularIBL, std::shared_ptr<Texture> specularBRDF) {
  std::unique_lock<std::mutex> lock(_mutex);
  _textures[MaterialTexture::IBL_SPECULAR] = {specularIBL};
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _changedTextures[MaterialTexture::IBL_SPECULAR][i] = true;
  }
  _textures[MaterialTexture::BRDF_SPECULAR] = {specularBRDF};
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _changedTextures[MaterialTexture::BRDF_SPECULAR][i] = true;
  }
}

void MaterialPBR::setCoefficients(float metallicFactor,
                                  float roughnessFactor,
                                  float occlusionStrength,
                                  glm::vec3 emissiveFactor) {
  std::unique_lock<std::mutex> lock(_mutex);
  _coefficients = {.metallicFactor = metallicFactor,
                   .roughnessFactor = roughnessFactor,
                   .occlusionStrength = occlusionStrength,
                   .emissiveFactor = emissiveFactor};
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) _changedCoefficients[i] = true;
}

MaterialPhong::MaterialPhong(MaterialTarget target,
                             std::shared_ptr<CommandBuffer> commandBufferTransfer,
                             std::shared_ptr<EngineState> engineState)
    : Material(commandBufferTransfer, engineState) {
  _target = target;

  _uniformBufferCoefficients.resize(_engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++)
    _uniformBufferCoefficients[i] = std::make_shared<Buffer>(
        sizeof(Coefficients), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, engineState);
  _changedTextures[MaterialTexture::COLOR].resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
  _changedTextures[MaterialTexture::NORMAL].resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
  _changedTextures[MaterialTexture::SPECULAR].resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
  _changedCoefficients.resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
  // set default coefficients
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _updateCoefficientBuffer(i);
  }
}

void MaterialPhong::_updateCoefficientBuffer(int currentFrame) {
  _uniformBufferCoefficients[currentFrame]->setData(&_coefficients);
}

const std::vector<std::shared_ptr<Texture>> MaterialPhong::getBaseColor() { return _textures[MaterialTexture::COLOR]; }

const std::vector<std::shared_ptr<Texture>> MaterialPhong::getNormal() { return _textures[MaterialTexture::NORMAL]; }

const std::vector<std::shared_ptr<Texture>> MaterialPhong::getSpecular() {
  return _textures[MaterialTexture::SPECULAR];
}

void MaterialPhong::setBaseColor(std::vector<std::shared_ptr<Texture>> color) {
  std::unique_lock<std::mutex> lock(_mutex);
  if (static_cast<int>(_target) != color.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textures[MaterialTexture::COLOR] = color;
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _changedTextures[MaterialTexture::COLOR][i] = true;
  }
}

void MaterialPhong::setSpecular(std::vector<std::shared_ptr<Texture>> specular) {
  std::unique_lock<std::mutex> lock(_mutex);
  if (static_cast<int>(_target) != specular.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textures[MaterialTexture::SPECULAR] = specular;

  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _changedTextures[MaterialTexture::SPECULAR][i] = true;
  }
}

void MaterialPhong::setNormal(std::vector<std::shared_ptr<Texture>> normal) {
  std::unique_lock<std::mutex> lock(_mutex);
  if (static_cast<int>(_target) != normal.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textures[MaterialTexture::NORMAL] = normal;

  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _changedTextures[MaterialTexture::NORMAL][i] = true;
  }
}

void MaterialPhong::setCoefficients(glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, float shininess) {
  std::unique_lock<std::mutex> lock(_mutex);
  _coefficients = {._ambient = ambient, ._diffuse = diffuse, ._specular = specular, ._shininess = shininess};
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) _changedCoefficients[i] = true;
}

MaterialColor::MaterialColor(MaterialTarget target,
                             std::shared_ptr<CommandBuffer> commandBufferTransfer,
                             std::shared_ptr<EngineState> engineState)
    : Material(commandBufferTransfer, engineState) {
  _target = target;
  _changedCoefficients.resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
  _changedTextures[MaterialTexture::COLOR].resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
}

void MaterialColor::_updateCoefficientBuffer(int currentFrame) {}

const std::vector<std::shared_ptr<Texture>> MaterialColor::getBaseColor() { return _textures[MaterialTexture::COLOR]; }

void MaterialColor::setBaseColor(std::vector<std::shared_ptr<Texture>> color) {
  std::unique_lock<std::mutex> lock(_mutex);
  if (static_cast<int>(_target) != color.size())
    throw std::invalid_argument("expected size of vector is " + std::to_string(static_cast<int>(_target)));
  _textures[MaterialTexture::COLOR] = color;
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _changedTextures[MaterialTexture::COLOR][i] = true;
  }
}