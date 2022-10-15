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

Shader::Shader(std::string vertex, std::string fragment, std::shared_ptr<Device> device) {
  _device = device;

  auto vertShaderCode = readFile(vertex);
  auto fragShaderCode = readFile(fragment);
  VkShaderModule vertShaderModule = _createShaderModule(vertShaderCode);
  VkShaderModule fragShaderModule = _createShaderModule(fragShaderCode);

  _vertShaderStageInfo = {};
  _vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  _vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  _vertShaderStageInfo.module = vertShaderModule;
  _vertShaderStageInfo.pName = "main";

  _fragShaderStageInfo = {};
  _fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  _fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  _fragShaderStageInfo.module = fragShaderModule;
  _fragShaderStageInfo.pName = "main";
}

VkPipelineShaderStageCreateInfo& Shader::getFragShaderStageInfo() { return _fragShaderStageInfo; }

VkPipelineShaderStageCreateInfo& Shader::getVertShaderStageInfo() { return _vertShaderStageInfo; }

Shader::~Shader() {
  vkDestroyShaderModule(_device->getLogicalDevice(), _vertShaderStageInfo.module, nullptr);
  vkDestroyShaderModule(_device->getLogicalDevice(), _fragShaderStageInfo.module, nullptr);
}