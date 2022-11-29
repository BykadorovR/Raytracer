#pragma once
#include "Device.h"
#include <map>

class Shader {
 private:
  std::shared_ptr<Device> _device;
  std::map<VkShaderStageFlagBits, VkPipelineShaderStageCreateInfo> _shaders;
  VkShaderModule _createShaderModule(const std::vector<char>& code);

 public:
  Shader(std::shared_ptr<Device> device);
  void add(std::string text, VkShaderStageFlagBits type);
  VkPipelineShaderStageCreateInfo& getShaderStageInfo(VkShaderStageFlagBits type);
  ~Shader();
};