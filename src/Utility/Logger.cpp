#include "Logger.h"
#include "nvtx3/nvtx3.hpp"

LoggerCPU::LoggerCPU() {}

void LoggerCPU::begin(std::string name) { nvtxRangePush(name.c_str()); }

void LoggerCPU::end() { nvtxRangePop(); }

void LoggerCPU::mark(std::string name) { nvtx3::mark(name); }

LoggerGPU::LoggerGPU(std::shared_ptr<CommandBuffer> buffer, std::shared_ptr<State> state) {
  _state = state;
  _buffer = buffer;

  _cmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(
      state->getInstance()->getInstance(), "vkCmdBeginDebugUtilsLabelEXT");
  _cmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(state->getInstance()->getInstance(),
                                                                                    "vkCmdEndDebugUtilsLabelEXT");
  _setDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(
      state->getInstance()->getInstance(), "vkSetDebugUtilsObjectNameEXT");
}

void LoggerGPU::initialize(std::string name, int currentFrame) {
  VkDebugUtilsObjectNameInfoEXT cmdBufInfo = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .pNext = NULL,
      .objectType = VK_OBJECT_TYPE_COMMAND_BUFFER,
      .objectHandle = (uint64_t)_buffer->getCommandBuffer()[currentFrame],
      .pObjectName = name.c_str(),
  };
  _setDebugUtilsObjectNameEXT(_state->getDevice()->getLogicalDevice(), &cmdBufInfo);
}

void LoggerGPU::begin(std::string name, int currentFrame) {
  VkDebugUtilsLabelEXT markerInfo = {};
  markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  markerInfo.pLabelName = name.c_str();
  _cmdBeginDebugUtilsLabelEXT(_buffer->getCommandBuffer()[currentFrame], &markerInfo);
}

void LoggerGPU::end(int currentFrame) { _cmdEndDebugUtilsLabelEXT(_buffer->getCommandBuffer()[currentFrame]); }
