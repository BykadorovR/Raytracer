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
  VkClearColorValue _clearColor = {0.2f, 0.2f, 0.2f, 1.f};
  std::string _name = "default";
  VkFormat _colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
  VkFormat _depthFormat = VK_FORMAT_D32_SFLOAT;
  // if changed have to be change in shaders too
  int _threadsInPool = 6;
  int _maxDirectionalLights = 2;
  int _maxPointLights = 4;
  std::vector<std::tuple<int, float, float, float>> _attenuations = {
      {7, 1.0, 0.7, 1.8},        {13, 1.0, 0.35, 0.44},     {20, 1.0, 0.22, 0.20},     {32, 1.0, 0.14, 0.07},
      {50, 1.0, 0.09, 0.032},    {65, 1.0, 0.07, 0.017},    {100, 1.0, 0.045, 0.0075}, {160, 1.0, 0.027, 0.0028},
      {200, 1.0, 0.022, 0.0019}, {325, 1.0, 0.014, 0.0007}, {600, 1.0, 0.007, 0.0002}, {3250, 1.0, 0.0014, 0.000007}};

 public:
  // setters
  void setName(std::string name);
  void setResolution(std::tuple<int, int> resolution);
  void setDepthResolution(std::tuple<int, int> depthResolution);
  void setColorFormat(VkFormat format);
  void setDepthFormat(VkFormat format);
  void setMaxFramesInFlight(int maxFramesInFlight);
  void setThreadsInPool(int threadsInPool);
  void setClearColor(VkClearColorValue clearColor);
  // getters
  const std::tuple<int, int>& getResolution();
  const std::tuple<int, int>& getDepthResolution();
  std::string getName();
  int getMaxFramesInFlight();
  int getMaxDirectionalLights();
  int getMaxPointLights();
  std::vector<std::tuple<int, float, float, float>> getAttenuations();
  int getThreadsInPool();
  VkFormat getColorFormat();
  VkFormat getDepthFormat();
  VkClearColorValue getClearColor();
};
