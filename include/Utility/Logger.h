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
  std::shared_ptr<CommandBuffer> _buffer;
  std::shared_ptr<State> _state;
  int _currentFrame;

 public:
  LoggerGPU(std::shared_ptr<State> state);
  void setCommandBufferName(std::string bufferName, int currentFrame, std::shared_ptr<CommandBuffer> buffer);
  void begin(std::string marker, int currentFrame);
  void end();
};