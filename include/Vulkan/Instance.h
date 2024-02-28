#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <exception>
#include <stdexcept>
#include <memory>
#include <iostream>
#include "Window.h"

class Instance {
 private:
  VkInstance _instance;
  VkDebugUtilsMessengerEXT _debugMessenger;
  // validation
  bool _validation = false;
  const std::vector<const char*> _validationLayers = {"VK_LAYER_KHRONOS_validation"};
  bool _checkValidationLayersSupport();

 public:
  Instance(std::string name, bool validation, std::shared_ptr<Window> window);
  const VkInstance& getInstance();
  const std::vector<const char*>& getValidationLayers();
  bool isValidation();
  ~Instance();
};