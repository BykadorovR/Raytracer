#include "Logger.h"
#include "nvtx3/nvtx3.hpp"

LoggerCPU::LoggerCPU() {}

void LoggerCPU::begin(std::string name) { nvtxRangePush(name.c_str()); }

void LoggerCPU::end() { nvtxRangePop(); }

void LoggerCPU::mark(std::string name) { nvtx3::mark(name); }

LoggerGPU::LoggerGPU(std::shared_ptr<State> state) {
  _state = state;

  _cmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(
      state->getInstance()->getInstance(), "vkCmdBeginDebugUtilsLabelEXT");
  _cmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(state->getInstance()->getInstance(),
                                                                                    "vkCmdEndDebugUtilsLabelEXT");
  _setDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(
      state->getInstance()->getInstance(), "vkSetDebugUtilsObjectNameEXT");
}

void LoggerGPU::setCommandBufferName(std::string name, int currentFrame, std::shared_ptr<CommandBuffer> buffer) {
  _buffer = buffer;
  VkDebugUtilsObjectNameInfoEXT cmdBufInfo = {};
  cmdBufInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
  cmdBufInfo.pNext = nullptr;
  cmdBufInfo.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
  cmdBufInfo.objectHandle = (uint64_t)_buffer->getCommandBuffer()[currentFrame];
  cmdBufInfo.pObjectName = name.c_str();
  _setDebugUtilsObjectNameEXT(_state->getDevice()->getLogicalDevice(), &cmdBufInfo);
}

void LoggerGPU::begin(std::string name, int currentFrame) {
  _currentFrame = currentFrame;

  VkDebugUtilsLabelEXT markerInfo = {};
  markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  markerInfo.pLabelName = name.c_str();
  _cmdBeginDebugUtilsLabelEXT(_buffer->getCommandBuffer()[currentFrame], &markerInfo);
}

void LoggerGPU::end() { _cmdEndDebugUtilsLabelEXT(_buffer->getCommandBuffer()[_currentFrame]); }