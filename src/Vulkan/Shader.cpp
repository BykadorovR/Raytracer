#include "Shader.h"
#include "Utils.h"

VkShaderModule Shader::_createShaderModule(const std::vector<char>& code) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(_device->getLogicalDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
    throw std::runtime_error("failed to create shader module!");
  }

  return shaderModule;
}

Shader::Shader(std::shared_ptr<Device> device) { _device = device; }

void Shader::add(std::string path, VkShaderStageFlagBits type) {
  auto shaderCode = readFile(path);
  VkShaderModule shaderModule = _createShaderModule(shaderCode);
  _shaders[type] = {};
  _shaders[type].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  _shaders[type].stage = type;
  _shaders[type].module = shaderModule;
  _shaders[type].pName = "main";
}

void Shader::setSpecializationInfo(VkSpecializationInfo info, VkShaderStageFlagBits type) {
  _specializationInfo = info;
  _shaders[type].pSpecializationInfo = &_specializationInfo;
}

VkPipelineShaderStageCreateInfo& Shader::getShaderStageInfo(VkShaderStageFlagBits type) { return _shaders[type]; }

Shader::~Shader() {
  for (auto& [type, shader] : _shaders) vkDestroyShaderModule(_device->getLogicalDevice(), shader.module, nullptr);
}