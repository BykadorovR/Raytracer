#pragma once
#include "State.h"

class LoggerCPU {
 public:
  LoggerCPU();
  void begin(std::string name);
  void end();
  void mark(std::string name);
};

class LoggerGPU {
 private:
  PFN_vkCmdBeginDebugUtilsLabelEXT _cmdBeginDebugUtilsLabelEXT;
  PFN_vkCmdEndDebugUtilsLabelEXT _cmdEndDebugUtilsLabelEXT;
  PFN_vkSetDebugUtilsObjectNameEXT _setDebugUtilsObjectNameEXT;

  std::shared_ptr<State> _state;

 public:
  LoggerGPU(std::shared_ptr<State> state);
  void initialize(std::string bufferName, int currentFrame);
  void begin(std::string marker, int currentFrame);
  void end(int currentFrame);
};