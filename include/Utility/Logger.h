#pragma once
#include "State.h"
#include "Command.h"
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

class DebuggerUtils {
  PFN_vkSetDebugUtilsObjectNameEXT _setDebugUtilsObjectNameEXT;
  PFN_vkSetDebugUtilsObjectTagEXT _setDebugUtilsObjectTagEXT;
  std::shared_ptr<Device> _device;

 public:
  DebuggerUtils(std::shared_ptr<Instance> instance, std::shared_ptr<Device> device);
  template <class T>
  void setName(std::string name, VkObjectType type, std::vector<T> handlers) {
    for (int i = 0; i < handlers.size(); i++) {
      std::string modified = name + " " + std::to_string(i);
      VkDebugUtilsObjectNameInfoEXT nameInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
      nameInfo.objectType = type;
      nameInfo.objectHandle = (uint64_t)handlers[i];
      nameInfo.pObjectName = modified.c_str();
      _setDebugUtilsObjectNameEXT(_device->getLogicalDevice(), &nameInfo);
    }
  }

  template <class T>
  void setTag(std::string tag, VkObjectType type, std::vector<T> handlers) {
    for (int i = 0; i < handlers.size(); i++) {
      std::string modified = tag + " " + std::to_string(i);
      VkDebugUtilsObjectTagInfoEXT tagInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT};
      tagInfo.objectType = type;
      tagInfo.objectHandle = (uint64_t)handlers[i];
      tagInfo.tagName = 0;
      tagInfo.tagSize = modified.size();
      tagInfo.pTag = modified.c_str();
      _setDebugUtilsObjectTagEXT(_device->getLogicalDevice(), &tagInfo);
    }
  }
};