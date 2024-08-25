#pragma once
#include "Utility/State.h"
#include "Vulkan/Command.h"
#include <array>

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
  void begin(std::string marker, std::shared_ptr<CommandBuffer> buffer, std::array<float, 4> color);
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
  void begin(std::string marker,
             std::shared_ptr<CommandBuffer> buffer = nullptr,
             std::array<float, 4> color = {0.0f, 0.0f, 0.0f, 0.0f});
  void end(std::shared_ptr<CommandBuffer> buffer = nullptr);
};