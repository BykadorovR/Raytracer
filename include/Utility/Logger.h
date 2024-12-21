#pragma once
#include "Utility/EngineState.h"
#include "Vulkan/Command.h"
#include <array>

class LoggerAndroid {
 public:
  void begin(std::string name);
  void end();
};

class LoggerUtils {
 private:
  std::shared_ptr<EngineState> _engineState;

 public:
  LoggerUtils(std::shared_ptr<EngineState> engineState);
  template <class T>
  void setName(std::string name, VkObjectType type, T handler) {
    VkDebugUtilsObjectNameInfoEXT nameInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
    nameInfo.objectType = type;
    nameInfo.objectHandle = (uint64_t)handler;
    nameInfo.pObjectName = name.c_str();
    vkSetDebugUtilsObjectNameEXT(_engineState->getDevice()->getLogicalDevice(), &nameInfo);
  }

  template <class T>
  void setName(std::string name, VkObjectType type, std::vector<T> handlers) {
    for (int i = 0; i < handlers.size(); i++) {
      std::string modified = name + " " + std::to_string(i);
      setName(modified, type, handlers[i]);
    }
  }

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
  std::shared_ptr<EngineState> _engineState;

 public:
  Logger(std::shared_ptr<EngineState> engineState = nullptr);
  void begin(std::string marker,
             std::shared_ptr<CommandBuffer> buffer = nullptr,
             std::array<float, 4> color = {0.0f, 0.0f, 0.0f, 0.0f});
  void end(std::shared_ptr<CommandBuffer> buffer = nullptr);
};