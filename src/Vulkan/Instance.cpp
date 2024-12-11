#include "Vulkan/Instance.h"
#include <iostream>
#ifdef __ANDROID__
#include <android/log.h>
#endif

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallbackUtils(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                  void* pUserData) {
#ifdef __ANDROID__
  __android_log_print(ANDROID_LOG_ERROR, "validation layer: ", "%s", pCallbackData->pMessage);
#else
  std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
#endif
  return VK_FALSE;
}

Instance::Instance(std::string name, bool validation) {
  // VK_KHR_win32_surface || VK_KHR_android_surface as well as VK_KHR_surface
  // are automatically added by vkb
  vkb::InstanceBuilder builder;
  auto systemInfoResult = vkb::SystemInfo::get_system_info();
  if (!systemInfoResult) throw std::runtime_error(systemInfoResult.error().message());
  auto systemInfo = systemInfoResult.value();
  // debug utils is a newer API, can be not supported. Includes validation layers + API for code instrumentation.
  // debug report is deprecated, includes only validation layers.
  if (validation && systemInfo.validation_layers_available) {
    builder.enable_validation_layers();
    if (systemInfo.debug_utils_available) {
      builder.set_debug_callback(debugCallbackUtils);
      _debugUtils = true;
    }
  }
  auto instanceResult = builder.set_app_name(name.c_str()).require_api_version(1, 1, 0).build();
  if (!instanceResult)
    throw std::runtime_error("Failed to create Vulkan instance. Error: " + instanceResult.error().message());

  _instance = instanceResult.value();
}

bool Instance::isDebug() { return _debugUtils; }

const vkb::Instance& Instance::getInstance() { return _instance; }

Instance::~Instance() { vkb::destroy_instance(_instance); }