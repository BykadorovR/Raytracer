#include "Utility/Logger.h"
#include "nvtx3/nvtx3.hpp"
#ifdef __ANDROID__
#include <android/trace.h>
#endif

DebugUtils::DebugUtils(std::shared_ptr<Device> device) { _device = device; }

void Logger::LoggerAndroid::begin(std::string name) {
#ifdef __ANDROID__
  ATrace_beginSection(name.c_str());
#endif
}

void Logger::LoggerAndroid::end() {
#ifdef __ANDROID__
  ATrace_endSection();
#endif
}

Logger::LoggerUtils::LoggerUtils(std::shared_ptr<Device> device) { _device = device; }

void Logger::LoggerUtils::begin(std::string marker, std::shared_ptr<CommandBuffer> buffer, std::array<float, 4> color) {
  VkDebugUtilsLabelEXT markerInfo = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, .pLabelName = marker.c_str()};
  std::copy(color.begin(), color.end(), markerInfo.color);
  vkCmdBeginDebugUtilsLabelEXT(buffer->getCommandBuffer(), &markerInfo);
}

void Logger::LoggerUtils::end(std::shared_ptr<CommandBuffer> buffer) {
  vkCmdEndDebugUtilsLabelEXT(buffer->getCommandBuffer());
}

void Logger::LoggerNVTX::begin(std::string name) { nvtxRangePush(name.c_str()); }

void Logger::LoggerNVTX::end() { nvtxRangePop(); }

Logger::Logger(std::shared_ptr<Device> device) {
#ifdef __ANDROID__
  _loggerAndroid = std::make_shared<LoggerAndroid>();
#else
  if (device) _loggerUtils = std::make_shared<LoggerUtils>(device);
  _loggerNVTX = std::make_shared<LoggerNVTX>();
#endif
}

void Logger::begin(std::string marker, std::shared_ptr<CommandBuffer> buffer, std::array<float, 4> color) {
  std::unique_lock<std::mutex> lock(_mutex);
#ifdef __ANDROID__
  _loggerAndroid->begin(marker);
#else
  if (buffer)
    _loggerUtils->begin(marker, buffer, color);
  else
    _loggerNVTX->begin(marker);
#endif
}

void Logger::end(std::shared_ptr<CommandBuffer> buffer) {
  std::unique_lock<std::mutex> lock(_mutex);
#ifdef __ANDROID__
  _loggerAndroid->end();
#else
  if (buffer)
    _loggerUtils->end(buffer);
  else
    _loggerNVTX->end();
#endif
}