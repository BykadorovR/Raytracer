#pragma once
#include "State.h"

class MaterialPhong {
 private:
  struct Coefficients {
    alignas(16) glm::vec3 _ambient{0.2f};
    alignas(16) glm::vec3 _diffuse{1.f};
    alignas(16) glm::vec3 _specular{0.5f};
    float _shininess{64.f};
  };

  struct AlphaCutoff {
    bool alphaMask = false;
    float alphaCutoff = 0.f;
  };

  AlphaCutoff _alphaCutoff;
  bool _doubleSided = false;

  std::shared_ptr<Texture> _textureColor;
  std::shared_ptr<Texture> _textureNormal;
  std::shared_ptr<Texture> _stubTextureZero, _stubTextureOne;
  glm::vec4 _baseColor{1.f};

  Coefficients _coefficients;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayoutCoefficients, _descriptorSetLayoutTextures;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayoutAlphaCutoff;
  std::shared_ptr<DescriptorSet> _descriptorSetCoefficients, _descriptorSetTextures;
  std::shared_ptr<DescriptorSet> _descriptorSetAlphaCutoff;
  std::shared_ptr<UniformBuffer> _uniformBufferCoefficients;
  std::shared_ptr<UniformBuffer> _uniformBufferAlphaCutoff;
  std::shared_ptr<State> _state;
  std::vector<bool> _changedTexture;
  std::vector<bool> _changedCoefficients;
  std::mutex _accessMutex;

  void _updateTextureDescriptors(int currentFrame);
  void _updateCoefficientDescriptors(int currentFrame);
  void _updateAlphaCutoffDescriptors(int currentFrame);

 public:
  MaterialPhong(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state);

  void setDoubleSided(bool doubleSided);
  void setAlphaCutoff(bool alphaCutoff, float alphaMask);
  void setBaseColor(std::shared_ptr<Texture> color);
  void setNormal(std::shared_ptr<Texture> normal);
  void setPhongCoefficients(glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, float shininess);
  bool getDoubleSided();

  std::shared_ptr<DescriptorSet> getDescriptorSetAlphaCutoff();
  std::shared_ptr<DescriptorSetLayout> getDescriptorSetLayoutAlphaCutoff();
  std::shared_ptr<DescriptorSet> getDescriptorSetCoefficients(int currentFrame);
  std::shared_ptr<DescriptorSetLayout> getDescriptorSetLayoutCoefficients();
  std::shared_ptr<DescriptorSet> getDescriptorSetTextures(int currentFrame);
  std::shared_ptr<DescriptorSetLayout> getDescriptorSetLayoutTextures();
};