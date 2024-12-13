#pragma once
#include "volk.h"
#include <string>
#include <vector>
#include "VkBootstrap.h"

class Instance {
 private:
  vkb::Instance _instance;
  bool _debugUtils = false;

 public:
  Instance(std::string name, bool validation);
  bool isDebug();
  const vkb::Instance& getInstance();
  ~Instance();
};