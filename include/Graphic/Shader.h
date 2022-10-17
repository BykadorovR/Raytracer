#pragma once
#include "Device.h"

class Shader {
 private:
  std::shared_ptr<Device> _device;
  VkPipelineShaderStageCreateInfo _fragShaderStageInfo, _vertShaderStageInfo;

  VkShaderModule _createShaderModule(const std::vector<char>& code);

 public:
  Shader(std::string vertex, std::string fragment, std::shared_ptr<Device> device);

  VkPipelineShaderStageCreateInfo& getFragShaderStageInfo();
  VkPipelineShaderStageCreateInfo& getVertShaderStageInfo();

  ~Shader();
};