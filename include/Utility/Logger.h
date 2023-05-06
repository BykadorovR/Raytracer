#pragma once
#include "State.h"

class Logger {
 private:
  PFN_vkCmdBeginDebugUtilsLabelEXT _cmdBeginDebugUtilsLabelEXT;
  PFN_vkCmdEndDebugUtilsLabelEXT _cmdEndDebugUtilsLabelEXT;
  PFN_vkSetDebugUtilsObjectNameEXT _setDebugUtilsObjectNameEXT;

  std::shared_ptr<State> _state;

 public:
  Logger(std::shared_ptr<State> state);
  void setDebugUtils(std::string name, int currentFrame);
  void beginDebugUtils(std::string marker, int currentFrame);
  void endDebugUtils(int currentFrame);
};