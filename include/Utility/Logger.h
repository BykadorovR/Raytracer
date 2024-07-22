#pragma once
#include "State.h"
#include "Command.h"

class LoggerAndroid {
 public:
  void begin(std::string name);
  void end();
};

class LoggerUtils {
 private:
  PFN_vkCmdBeginDebugUtilsLabelEXT _cmdBeginDebugUtilsLabelEXT;
  PFN_vkCmdEndDebugUtilsLabelEXT _cmdEndDebugUtilsLabelEXT;
  std::shared_ptr<State> _state;

 public:
  LoggerUtils(std::shared_ptr<State> state);
  void begin(std::string marker, std::shared_ptr<CommandBuffer> buffer);
  void end(std::shared_ptr<CommandBuffer> buffer);
};

class LoggerNVTX {
 public:
  void begin(std::string name);
  void end();
};

class Logger {
 private:
  std::shared_ptr<LoggerAndroid> _loggerAndroid;
  std::shared_ptr<LoggerUtils> _loggerUtils;
  std::shared_ptr<LoggerNVTX> _loggerNVTX;
  std::shared_ptr<State> _state;

 public:
  Logger(std::shared_ptr<State> state = nullptr);
  void begin(std::string marker, std::shared_ptr<CommandBuffer> buffer = nullptr);
  void end(std::shared_ptr<CommandBuffer> buffer = nullptr);
};