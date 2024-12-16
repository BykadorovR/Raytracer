#include "Utility/Logger.h"
#include "nvtx3/nvtx3.hpp"
#ifdef __ANDROID__
#include <android/trace.h>
#endif

void LoggerAndroid::begin(std::string name) {
#ifdef __ANDROID__
  ATrace_beginSection(name.c_str());
#endif
}

void LoggerAndroid::end() {
#ifdef __ANDROID__
  ATrace_endSection();
#endif
}

LoggerUtils::LoggerUtils(std::shared_ptr<EngineState> engineState) { _engineState = engineState; }

void LoggerUtils::begin(std::string marker, std::shared_ptr<CommandBuffer> buffer, std::array<float, 4> color) {
  auto frameInFlight = _engineState->getFrameInFlight();
  VkDebugUtilsLabelEXT markerInfo = {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, .pLabelName = marker.c_str()};
  std::copy(color.begin(), color.end(), markerInfo.color);
  vkCmdBeginDebugUtilsLabelEXT(buffer->getCommandBuffer()[frameInFlight], &markerInfo);
}

void LoggerUtils::end(std::shared_ptr<CommandBuffer> buffer) {
  auto frameInFlight = _engineState->getFrameInFlight();
  vkCmdEndDebugUtilsLabelEXT(buffer->getCommandBuffer()[frameInFlight]);
}

void LoggerNVTX::begin(std::string name) { nvtxRangePush(name.c_str()); }

void LoggerNVTX::end() { nvtxRangePop(); }

Logger::Logger(std::shared_ptr<EngineState> engineState) {
#ifdef __ANDROID__
  _loggerAndroid = std::make_shared<LoggerAndroid>();
#else
  _engineState = engineState;
  if (engineState)
    _loggerUtils = std::make_shared<LoggerUtils>(engineState);
  else
    _loggerNVTX = std::make_shared<LoggerNVTX>();
#endif
}

void Logger::begin(std::string marker, std::shared_ptr<CommandBuffer> buffer, std::array<float, 4> color) {
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
#ifdef __ANDROID__
  _loggerAndroid->end();
#else
  if (buffer)
    _loggerUtils->end(buffer);
  else
    _loggerNVTX->end();
#endif
}