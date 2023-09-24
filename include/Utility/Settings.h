#pragma once
#include <tuple>
#include <string>
#include "vulkan/vulkan.hpp"

struct Settings {
 private:
  int _maxFramesInFlight;
  std::tuple<int, int> _resolution = {1920, 1080};
  std::tuple<int, int> _depthResolution = {1024, 1024};
  // VkClearColorValue _clearColor = {196.f / 255.f, 233.f / 255.f, 242.f / 255.f, 1.f};
  VkClearColorValue _clearColor = {0.0f, 0.0f, 0.0f, 1.f};
  std::string _name = "default";
  VkFormat _swapchainColorFormat;
  VkFormat _graphicColorFormat;
  VkFormat _loadTextureColorFormat;
  VkFormat _loadTextureAuxilaryFormat;
  VkFormat _depthFormat = VK_FORMAT_D32_SFLOAT;
  // if changed have to be change in shaders too
  int _threadsInPool = 6;
  int _maxDirectionalLights = 2;
  int _maxPointLights = 4;
  int _anisotropicSamples = 0;
  // TODO: protect by mutex?
  int _bloomPasses = 1;
  int _desiredFPS;
  std::vector<std::tuple<int, float>> _attenuations = {{7, 1.8},      {13, 0.44},    {20, 0.20},    {32, 0.07},
                                                       {50, 0.032},   {65, 0.017},   {100, 0.0075}, {160, 0.0028},
                                                       {200, 0.0019}, {325, 0.0007}, {600, 0.0002}, {3250, 0.000007}};

 public:
  // setters
  void setName(std::string name);
  void setResolution(std::tuple<int, int> resolution);
  void setDepthResolution(std::tuple<int, int> depthResolution);
  void setLoadTextureColorFormat(VkFormat format);
  void setLoadTextureAuxilaryFormat(VkFormat format);
  void setGraphicColorFormat(VkFormat format);
  void setSwapchainColorFormat(VkFormat format);
  void setGuiColorFormat(VkFormat format);
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
};
