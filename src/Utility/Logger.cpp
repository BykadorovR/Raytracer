#include "Logger.h"

Logger::Logger(std::shared_ptr<State> state) {
  _state = state;
  _cmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(
      state->getInstance()->getInstance(), "vkCmdBeginDebugUtilsLabelEXT");
  _cmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(state->getInstance()->getInstance(),
                                                                                    "vkCmdEndDebugUtilsLabelEXT");
  _setDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(
      state->getInstance()->getInstance(), "vkSetDebugUtilsObjectNameEXT");
}

void Logger::setDebugUtils(std::string name, int currentFrame) {
  VkDebugUtilsObjectNameInfoEXT cmdBufInfo = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .pNext = NULL,
      .objectType = VK_OBJECT_TYPE_COMMAND_BUFFER,
      .objectHandle = (uint64_t)_state->getCommandBuffer()->getCommandBuffer()[currentFrame],
      .pObjectName = name.c_str(),
  };
  _setDebugUtilsObjectNameEXT(_state->getDevice()->getLogicalDevice(), &cmdBufInfo);
}

void Logger::beginDebugUtils(std::string name, int currentFrame) {
  VkDebugUtilsLabelEXT markerInfo = {};
  markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  markerInfo.pLabelName = name.c_str();
  _cmdBeginDebugUtilsLabelEXT(_state->getCommandBuffer()->getCommandBuffer()[currentFrame], &markerInfo);
}

void Logger::endDebugUtils(int currentFrame) {
  _cmdEndDebugUtilsLabelEXT(_state->getCommandBuffer()->getCommandBuffer()[currentFrame]);
}
