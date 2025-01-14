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

void Logger::LoggerNVTX::begin(std::string name, std::array<float, 4> color) {
  nvtxEventAttributes_t eventAttrib = {0};
  eventAttrib.version = NVTX_VERSION;
  eventAttrib.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE;

  eventAttrib.colorType = NVTX_COLOR_ARGB;
  uint8_t a = color[3] * 255;  // Alpha
  uint8_t r = color[0] * 255;  // Red
  uint8_t g = color[1] * 255;  // Green
  uint8_t b = color[2] * 255;  // Blue
  uint32_t convertedColor = (a << 24) | (r << 16) | (g << 8) | b;
  eventAttrib.color = convertedColor;

  eventAttrib.messageType = NVTX_MESSAGE_TYPE_ASCII;
  eventAttrib.message.ascii = name.c_str();

  nvtxRangePushEx(&eventAttrib);
}

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
    _loggerNVTX->begin(marker, color);
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