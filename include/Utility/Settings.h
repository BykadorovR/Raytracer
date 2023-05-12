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

 public:
  Settings(std::string name, std::tuple<int, int> resolution, VkFormat format, int maxFramesInFlight);
  const std::tuple<int, int>& getResolution();
  std::string getName();
  int getMaxFramesInFlight();
  VkFormat getFormat();
};
