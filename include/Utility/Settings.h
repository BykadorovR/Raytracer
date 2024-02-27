#pragma once
#include <tuple>
#include <string>
#include "vulkan/vulkan.hpp"

enum class DrawType { FILL = 1, WIREFRAME = 2, NORMAL = 3, TANGENT = 4 };

inline DrawType operator|(DrawType a, DrawType b) {
  return static_cast<DrawType>(static_cast<int>(a) | static_cast<int>(b));
}

inline DrawType operator&(DrawType a, DrawType b) {
  return static_cast<DrawType>(static_cast<int>(a) & static_cast<int>(b));
}

inline DrawType operator~(DrawType a) { return (DrawType) ~(int)a; }

struct Settings {
 private:
  int _maxFramesInFlight;
  std::tuple<int, int> _resolution = {1920, 1080};
  std::tuple<int, int> _depthResolution = {512, 512};
  // used for irradiance diffuse cubemap generation
  std::tuple<int, int> _diffuseIBLResolution = {32, 32};
  std::tuple<int, int> _specularIBLResolution = {128, 128};
  int _specularIBLMipMap = 5;
  // VkClearColorValue _clearColor = {196.f / 255.f, 233.f / 255.f, 242.f / 255.f, 1.f};
  VkClearColorValue _clearColor = {0.0f, 0.0f, 0.0f, 1.f};
  std::string _name = "default";
  VkFormat _swapchainColorFormat;
  VkFormat _graphicColorFormat;
  // should be SRGB, so we convert to linear during loading
  VkFormat _loadTextureColorFormat;
  VkFormat _loadTextureAuxilaryFormat;
  VkFormat _depthFormat = VK_FORMAT_D32_SFLOAT;
  // if changed have to be change in shaders too
  int _threadsInPool = 6;
  int _maxDirectionalLights = 2;
  int _maxPointLights = 4;
  int _anisotropicSamples = 0;
  // TODO: protect by mutex?
  int _bloomPasses = 0;
  int _desiredFPS = 250;
  std::vector<std::tuple<int, float>> _attenuations = {{7, 1.8},      {13, 0.44},    {20, 0.20},    {32, 0.07},
                                                       {50, 0.032},   {65, 0.017},   {100, 0.0075}, {160, 0.0028},
                                                       {200, 0.0019}, {325, 0.0007}, {600, 0.0002}, {3250, 0.000007}};
  // Depth bias (and slope) are used to avoid shadowing artifacts
  // Constant depth bias factor (always applied)
  float _depthBiasConstant = 1.25f;
  // Slope depth bias factor, applied depending on polygon's slope
  float _depthBiasSlope = 1.75f;

 public:
  // setters
  void setName(std::string name);
  void setResolution(std::tuple<int, int> resolution);
  void setDepthResolution(std::tuple<int, int> depthResolution);
  void setLoadTextureColorFormat(VkFormat format);
  void setLoadTextureAuxilaryFormat(VkFormat format);
  void setGraphicColorFormat(VkFormat format);
  void setSwapchainColorFormat(VkFormat format);
  void setDepthFormat(VkFormat format);
  void setMaxFramesInFlight(int maxFramesInFlight);
  void setThreadsInPool(int threadsInPool);
  void setClearColor(VkClearColorValue clearColor);
  void setBloomPasses(int number);
  void setAnisotropicSamples(int number);
  void setDesiredFPS(int fps);
  // getters
  const std::tuple<int, int>& getResolution();
  const std::tuple<int, int>& getDepthResolution();
  std::string getName();
  int getMaxFramesInFlight();
  int getMaxDirectionalLights();
  int getMaxPointLights();
  std::vector<std::tuple<int, float>> getAttenuations();
  int getThreadsInPool();
  VkFormat getSwapchainColorFormat();
  VkFormat getGraphicColorFormat();
  VkFormat getLoadTextureColorFormat();
  VkFormat getLoadTextureAuxilaryFormat();
  VkFormat getDepthFormat();
  int getBloomPasses();
  VkClearColorValue getClearColor();
  int getAnisotropicSamples();
  int getDesiredFPS();
  std::tuple<int, int> getDiffuseIBLResolution();
  std::tuple<int, int> getSpecularIBLResolution();
  int getSpecularMipMap();
  float getDepthBiasConstant();
  float getDepthBiasSlope();
};
