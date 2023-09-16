#pragma once
#include "State.h"

class MaterialSpritePhong {
 private:
  struct Coefficients {
    alignas(16) glm::vec3 _ambient{0.2f};
    alignas(16) glm::vec3 _diffuse{1.f};
    alignas(16) glm::vec3 _specular{0.5f};
    float _shininess{64.f};
  };

  std::shared_ptr<Texture> _textureColor;
  std::shared_ptr<Texture> _textureNormal;
  std::shared_ptr<Texture> _stubTextureZero, _stubTextureOne;
  glm::vec4 _baseColor{1.f};

  Coefficients _coefficients;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayoutCoefficients, _descriptorSetLayoutTextures;
  std::shared_ptr<DescriptorSet> _descriptorSetCoefficients, _descriptorSetTextures;
  std::shared_ptr<UniformBuffer> _uniformBufferCoefficients;
  std::shared_ptr<State> _state;
  std::vector<bool> _changedTexture;
  std::vector<bool> _changedCoefficients;
  bool _changedBaseColor = false;
  std::mutex _accessMutex;

  void _updateTextureDescriptors(int currentFrame);
  void _updateCoefficientDescriptors(int currentFrame);
  void _updateBaseColor(std::shared_ptr<VertexBuffer<Vertex2D>> vertexBuffer);

 public:
  MaterialSpritePhong(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state);

  void setBaseColor(glm::vec4 color);
  void setBaseColor(std::shared_ptr<Texture> color);
  void setNormal(std::shared_ptr<Texture> normal);
  void setPhongCoefficients(glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, float shininess);

  std::shared_ptr<DescriptorSet> getDescriptorSetCoefficients();
  std::shared_ptr<DescriptorSetLayout> getDescriptorSetLayoutCoefficients();
  std::shared_ptr<DescriptorSet> getDescriptorSetTextures();
  std::shared_ptr<DescriptorSetLayout> getDescriptorSetLayoutTextures();

  void draw(int currentFrame, std::shared_ptr<VertexBuffer<Vertex2D>> vertexBuffer);
};