#include "Instance.h"
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

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallbackReport(VkDebugReportFlagsEXT flags,
                                                          VkDebugReportObjectTypeEXT /*type*/,
                                                          uint64_t /*object*/,
                                                          size_t /*location*/,
                                                          int32_t /*message_code*/,
                                                          const char* layerPrefix,
                                                          const char* message,
                                                          void* /*user_data*/) {
#ifdef __ANDROID__
  __android_log_print(ANDROID_LOG_ERROR, "validation layer: ", "%s", message);
#else
  std::cerr << "validation layer: " << message << std::endl;
#endif
  return VK_FALSE;
}

bool Instance::_checkValidationLayersSupport() {
  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  for (auto layerName : _validationLayers) {
    bool layerFound = false;

    for (const auto& layerProperties : availableLayers) {
      if (strcmp(layerName, layerProperties.layerName) == 0) {
        layerFound = true;
        break;
      }
    }

    if (!layerFound) {
      return false;
    }
  }

  return true;
}

Instance::Instance(std::string name, bool validation) {
  _validation = validation;

  if (_validation && _checkValidationLayersSupport() == false) {
    throw std::runtime_error("validation layers requested, but not available!");
  }

  uint32_t instanceExtensionCount;
  auto result = vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);
  if (result != VK_SUCCESS) throw std::runtime_error("Can't query instance extension properties");

  std::vector<VkExtensionProperties> instanceExtensionsAvailable(instanceExtensionCount);
  result = vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, instanceExtensionsAvailable.data());
  if (result != VK_SUCCESS) throw std::runtime_error("Can't query instance extension properties");

  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = name.c_str();
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "No Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_MAKE_API_VERSION(0, 1, 1, 0);

  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;

// extensions
#ifdef WIN32
  _extensionsInstance.push_back("VK_KHR_win32_surface");
#elif __ANDROID__
  _extensionsInstance.push_back("VK_KHR_android_surface");
#else
  throw std::runtime_error("Define surface extension for current platform!")
#endif

  if (_validation) {
    for (auto& availableExtension : instanceExtensionsAvailable) {
      if (strcmp(availableExtension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
        _debugUtils = true;
        _extensionsInstance.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        break;
      }
    }
    if (!_debugUtils) {
      _extensionsInstance.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }
  }
  createInfo.enabledExtensionCount = static_cast<uint32_t>(_extensionsInstance.size());
  createInfo.ppEnabledExtensionNames = _extensionsInstance.data();

  // validation
  createInfo.enabledLayerCount = static_cast<uint32_t>(_validationLayers.size());
  createInfo.ppEnabledLayerNames = _validationLayers.data();

  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfoUtils = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
  VkDebugReportCallbackCreateInfoEXT debugCreateInfoReport = {VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT};
  if (_validation) {
    if (_debugUtils) {
      debugCreateInfoUtils.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                             VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                             VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
      debugCreateInfoUtils.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                         VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                         VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
      debugCreateInfoUtils.pfnUserCallback = debugCallbackUtils;
      createInfo.pNext = &debugCreateInfoUtils;
    } else {
      debugCreateInfoReport.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
      debugCreateInfoReport.pfnCallback = debugCallbackReport;
      createInfo.pNext = &debugCreateInfoReport;
    }
  }

  if (vkCreateInstance(&createInfo, nullptr, &_instance) != VK_SUCCESS) {
    throw std::runtime_error("failed to create instance!");
  }

  if (_validation) {
    // if utils are supported
    if (_debugUtils) {
      auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance,
                                                                            "vkCreateDebugUtilsMessengerEXT");
      if (func == nullptr) throw std::runtime_error("failed to set up debug messenger!");
      result = func(_instance, &debugCreateInfoUtils, nullptr, &_debugMessenger);
      if (result != VK_SUCCESS) throw std::runtime_error("failed to set up debug messenger!");
    } else {
      // if report is supported
      auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(_instance,
                                                                            "vkCreateDebugReportCallbackEXT");
      if (func == nullptr) throw std::runtime_error("failed to set up debug messenger!");
      result = func(_instance, &debugCreateInfoReport, nullptr, &_debugReportCallback);
      if (result != VK_SUCCESS) throw std::runtime_error("failed to set up debug messenger!");
    }
  }
}

const VkInstance& Instance::getInstance() { return _instance; }

bool Instance::isValidation() { return _validation; }

bool Instance::isDebug() { return _debugUtils; }

const std::vector<const char*>& Instance::getValidationLayers() { return _validationLayers; }

Instance::~Instance() {
  if (_validation) {
    {
      auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(_instance,
                                                                             "vkDestroyDebugUtilsMessengerEXT");
      if (func != nullptr) {
        func(_instance, _debugMessenger, nullptr);
      }
    }
    {
      auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(_instance,
                                                                             "vkDestroyDebugReportCallbackEXT");
      if (func != nullptr) {
        func(_instance, _debugReportCallback, nullptr);
      }
    }
  }
  vkDestroyInstance(_instance, nullptr);
}