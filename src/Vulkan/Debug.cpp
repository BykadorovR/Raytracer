#include "Debug.h"

DebuggerUtils::DebuggerUtils(std::shared_ptr<Instance> instance, std::shared_ptr<Device> device) {
  _device = device;
  _instance = instance;

  if (instance->isDebug()) {
    _setDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(
        instance->getInstance(), "vkSetDebugUtilsObjectNameEXT");
  }
}