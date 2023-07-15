#include "Settings.h"

void Settings::setName(std::string name) { _name = name; }

void Settings::setResolution(std::tuple<int, int> resolution) { _resolution = resolution; }

void Settings::setDepthResolution(std::tuple<int, int> depthResolution) { _depthResolution = depthResolution; }

void Settings::setColorFormat(VkFormat format) { _colorFormat = format; }

void Settings::setDepthFormat(VkFormat format) { _depthFormat = format; }

void Settings::setMaxFramesInFlight(int maxFramesInFlight) { _maxFramesInFlight = maxFramesInFlight; }

void Settings::setThreadsInPool(int threadsInPool) { _threadsInPool = threadsInPool; }

std::string Settings::getName() { return _name; }

const std::tuple<int, int>& Settings::getResolution() { return _resolution; }

const std::tuple<int, int>& Settings::getDepthResolution() { return _depthResolution; }

int Settings::getMaxFramesInFlight() { return _maxFramesInFlight; }

VkFormat Settings::getColorFormat() { return _colorFormat; }

VkFormat Settings::getDepthFormat() { return _depthFormat; }

int Settings::getMaxDirectionalLights() { return _maxDirectionalLights; }

int Settings::getMaxPointLights() { return _maxPointLights; }

std::vector<std::tuple<int, float, float, float>> Settings::getAttenuations() { return _attenuations; }

int Settings::getThreadsInPool() { return _threadsInPool; }

VkClearColorValue Settings::getClearColor() { return _clearColor; }