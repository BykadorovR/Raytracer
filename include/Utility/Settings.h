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

 public:
  Settings(std::string name, std::tuple<int, int> resolution, VkFormat format, int maxFramesInFlight);
  const std::tuple<int, int>& getResolution();
  std::string getName();
  int getMaxFramesInFlight();
  VkFormat getFormat();
  int getMaxDirectionalLights();
  int getMaxPointLights();
};
