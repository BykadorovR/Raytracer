#pragma once
#include <tuple>
#include <string>
#include "vulkan/vulkan.hpp"

struct Settings {
 private:
  int _maxFramesInFlight;
  std::tuple<int, int> _resolution;
  std::string _name;
  VkFormat _format;
  // if changed have to be change in shaders too
  int _maxDirectionalLights = 2;
  int _maxPointLights = 4;
  std::vector<std::tuple<int, float, float, float>> _attenuations = {
      {7, 1.0, 0.7, 1.8},        {13, 1.0, 0.35, 0.44},     {20, 1.0, 0.22, 0.20},     {32, 1.0, 0.14, 0.07},
      {50, 1.0, 0.09, 0.032},    {65, 1.0, 0.07, 0.017},    {100, 1.0, 0.045, 0.0075}, {160, 1.0, 0.027, 0.0028},
      {200, 1.0, 0.022, 0.0019}, {325, 1.0, 0.014, 0.0007}, {600, 1.0, 0.007, 0.0002}, {3250, 1.0, 0.0014, 0.000007}};

 public:
  Settings(std::string name, std::tuple<int, int> resolution, VkFormat format, int maxFramesInFlight);
  const std::tuple<int, int>& getResolution();
  std::string getName();
  int getMaxFramesInFlight();
  VkFormat getFormat();
  int getMaxDirectionalLights();
  int getMaxPointLights();
  std::vector<std::tuple<int, float, float, float>> getAttenuations();
};
