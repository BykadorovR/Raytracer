#include "Settings.h"

void Settings::setName(std::string name) { _name = name; }

void Settings::setResolution(std::tuple<int, int> resolution) { _resolution = resolution; }

void Settings::setFormat(VkFormat format) { _format = format; }

void Settings::setMaxFramesInFlight(int maxFramesInFlight) { _maxFramesInFlight = maxFramesInFlight; }

void Settings::setThreadsInPool(int threadsInPool) { _threadsInPool = threadsInPool; }

std::string Settings::getName() { return _name; }

const std::tuple<int, int>& Settings::getResolution() { return _resolution; }

int Settings::getMaxFramesInFlight() { return _maxFramesInFlight; }

VkFormat Settings::getFormat() { return _format; }

int Settings::getMaxDirectionalLights() { return _maxDirectionalLights; }

int Settings::getMaxPointLights() { return _maxPointLights; }

std::vector<std::tuple<int, float, float, float>> Settings::getAttenuations() { return _attenuations; }

int Settings::getThreadsInPool() { return _threadsInPool; }