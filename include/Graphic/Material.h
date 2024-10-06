#pragma once
#include "Utility/EngineState.h"
#include "Vulkan/Descriptor.h"
#include "Primitive/Cubemap.h"

enum class MaterialType { PHONG, PBR, COLOR };

enum class MaterialTarget { SIMPLE = 1, TERRAIN = 4 };

enum class MaterialTexture {
  COLOR = 0,
  NORMAL,
  SPECULAR,
  METALLIC,
  ROUGHNESS,
  OCCLUSION,
  EMISSIVE,
  IBL_DIFFUSE,
  IBL_SPECULAR,
  BRDF_SPECULAR
};

class Material {
 protected:
  struct AlphaCutoff {
    bool alphaMask = false;
    float alphaCutoff = 0.f;
  };
  AlphaCutoff _alphaCutoff;
  bool _doubleSided = false;

  std::shared_ptr<UniformBuffer> _uniformBufferCoefficients;
  std::shared_ptr<UniformBuffer> _uniformBufferAlphaCutoff;
  std::map<std::shared_ptr<DescriptorSet>, std::vector<std::tuple<MaterialTexture, int>>> _descriptorsUpdate;
  std::shared_ptr<EngineState> _engineState;
  std::map<MaterialTexture, std::vector<bool>> _changedTextures;
  std::vector<bool> _changedCoefficients;
  std::vector<bool> _changedAlpha;
  std::mutex _mutex;
  std::map<MaterialTexture, std::vector<std::shared_ptr<Texture>>> _textures;

  void _updateAlphaCutoffDescriptors(int currentFrame);
  void _updateDescriptor(int currentFrame, MaterialTexture type);
  virtual void _updateCoefficientBuffer(int currentFrame) = 0;

 public:
  Material(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<EngineState> engineState);
  void registerUpdate(std::shared_ptr<DescriptorSet> descriptor, std::vector<std::tuple<MaterialTexture, int>> type);
  void unregisterUpdate(std::shared_ptr<DescriptorSet> descriptor);

  void setDoubleSided(bool doubleSided);
  void setAlphaCutoff(bool alphaCutoff, float alphaMask);
  bool getDoubleSided();
  std::shared_ptr<UniformBuffer> getBufferCoefficients();
  std::shared_ptr<UniformBuffer> getBufferAlphaCutoff();
  void update(int currentFrame);
};

class MaterialColor : public Material {
 protected:
  MaterialTarget _target;

 private:
  void _updateCoefficientBuffer(int currentFrame) override;

 public:
  // number of textures color
  MaterialColor(MaterialTarget target,
                std::shared_ptr<CommandBuffer> commandBufferTransfer,
                std::shared_ptr<EngineState> engineState);
  const std::vector<std::shared_ptr<Texture>> getBaseColor();

  void setBaseColor(std::vector<std::shared_ptr<Texture>> color);
};

class MaterialPBR : public MaterialColor {
 private:
  struct Coefficients {
    float metallicFactor{1};
    float roughnessFactor{1};
    // occludedColor = lerp(color, color * <sampled occlusion texture value>, <occlusion strength>)
    float occlusionStrength{0};
    alignas(16) glm::vec3 emissiveFactor{0};
  };

  Coefficients _coefficients;

  void _updateCoefficientBuffer(int currentFrame) override;

 public:
  // number of textures color, normal, metallic, roughness, occluded, emissive
  MaterialPBR(MaterialTarget target,
              std::shared_ptr<CommandBuffer> commandBufferTransfer,
              std::shared_ptr<EngineState> engineState);
  const std::vector<std::shared_ptr<Texture>> getNormal();
  const std::vector<std::shared_ptr<Texture>> getMetallic();
  const std::vector<std::shared_ptr<Texture>> getRoughness();
  const std::vector<std::shared_ptr<Texture>> getOccluded();
  const std::vector<std::shared_ptr<Texture>> getEmissive();
  std::shared_ptr<Texture> getDiffuseIBL();
  std::shared_ptr<Texture> getSpecularIBL();
  std::shared_ptr<Texture> getSpecularBRDF();

  void setNormal(std::vector<std::shared_ptr<Texture>> normal);
  void setMetallic(std::vector<std::shared_ptr<Texture>> metallic);
  void setRoughness(std::vector<std::shared_ptr<Texture>> roughness);
  void setOccluded(std::vector<std::shared_ptr<Texture>> occluded);
  void setEmissive(std::vector<std::shared_ptr<Texture>> emissive);
  void setDiffuseIBL(std::shared_ptr<Texture> diffuseIBL);
  void setSpecularIBL(std::shared_ptr<Texture> specularIBL, std::shared_ptr<Texture> specularBRDF);
  void setCoefficients(float metallicFactor, float roughnessFactor, float occlusionStrength, glm::vec3 emissiveFactor);
};

class MaterialPhong : public MaterialColor {
 private:
  struct Coefficients {
    alignas(16) glm::vec3 _ambient{0.2f};
    alignas(16) glm::vec3 _diffuse{1.f};
    alignas(16) glm::vec3 _specular{0.5f};
    float _shininess{64.f};
  };

  Coefficients _coefficients;

  void _updateCoefficientBuffer(int currentFrame) override;

 public:
  // number of textures color, normal and specular
  MaterialPhong(MaterialTarget target,
                std::shared_ptr<CommandBuffer> commandBufferTransfer,
                std::shared_ptr<EngineState> engineState);
  const std::vector<std::shared_ptr<Texture>> getNormal();
  const std::vector<std::shared_ptr<Texture>> getSpecular();

  void setNormal(std::vector<std::shared_ptr<Texture>> normal);
  void setSpecular(std::vector<std::shared_ptr<Texture>> specular);
  void setCoefficients(glm::vec3 ambient, glm::vec3 diffuse, glm::vec3 specular, float shininess);
};