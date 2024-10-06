#pragma once
#include "Utility/EngineState.h"

class DebuggerUtils {
  PFN_vkSetDebugUtilsObjectNameEXT _setDebugUtilsObjectNameEXT;
  std::shared_ptr<Device> _device;
  std::shared_ptr<Instance> _instance;

 public:
  DebuggerUtils(std::shared_ptr<Instance> instance, std::shared_ptr<Device> device);
  template <class T>
  void setName(std::string name, VkObjectType type, T handler) {
    if (_instance->isDebug()) {
      VkDebugUtilsObjectNameInfoEXT nameInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
      nameInfo.objectType = type;
      nameInfo.objectHandle = (uint64_t)handler;
      nameInfo.pObjectName = name.c_str();
      _setDebugUtilsObjectNameEXT(_device->getLogicalDevice(), &nameInfo);
    }
  }

  template <class T>
  void setName(std::string name, VkObjectType type, std::vector<T> handlers) {
    if (_instance->isDebug()) {
      for (int i = 0; i < handlers.size(); i++) {
        std::string modified = name + " " + std::to_string(i);
        setName(modified, type, handlers[i]);
      }
    }
  }
};