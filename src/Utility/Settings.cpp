#include "Settings.h"

void Settings::setName(std::string name) { _name = name; }

void Settings::setResolution(std::tuple<int, int> resolution) { _resolution = resolution; }

void Settings::setDepthResolution(std::tuple<int, int> depthResolution) { _depthResolution = depthResolution; }

void Settings::setGraphicColorFormat(VkFormat format) { _graphicColorFormat = format; }

void Settings::setLoadTextureColorFormat(VkFormat format) { _loadTextureColorFormat = format; }

void Settings::setLoadTextureAuxilaryFormat(VkFormat format) { _loadTextureAuxilaryFormat = format; }

void Settings::setSwapchainColorFormat(VkFormat format) { _swapchainColorFormat = format; }

void Settings::setDepthFormat(VkFormat format) { _depthFormat = format; }

void Settings::setMaxFramesInFlight(int maxFramesInFlight) { _maxFramesInFlight = maxFramesInFlight; }

void Settings::setAnisotropicSamples(int number) { _anisotropicSamples = number; }

void Settings::setDesiredFPS(int fps) { _desiredFPS = fps; }

void Settings::setBloomPasses(int number) { _bloomPasses = number; }

void Settings::setClearColor(VkClearColorValue clearColor) { _clearColor = clearColor; }

void Settings::setThreadsInPool(int threadsInPool) { _threadsInPool = threadsInPool; }

std::string Settings::getName() { return _name; }

const std::tuple<int, int>& Settings::getResolution() { return _resolution; }

const std::tuple<int, int>& Settings::getDepthResolution() { return _depthResolution; }

int Settings::getMaxFramesInFlight() { return _maxFramesInFlight; }

VkFormat Settings::getSwapchainColorFormat() { return _swapchainColorFormat; }

VkFormat Settings::getGraphicColorFormat() { return _graphicColorFormat; }

VkFormat Settings::getLoadTextureColorFormat() { return _loadTextureColorFormat; }

VkFormat Settings::getLoadTextureAuxilaryFormat() { return _loadTextureAuxilaryFormat; }

VkFormat Settings::getDepthFormat() { return _depthFormat; }

int Settings::getBloomPasses() { return _bloomPasses; }

int Settings::getMaxDirectionalLights() { return _maxDirectionalLights; }

int Settings::getMaxPointLights() { return _maxPointLights; }

std::vector<std::tuple<int, float>> Settings::getAttenuations() { return _attenuations; }

int Settings::getThreadsInPool() { return _threadsInPool; }

VkClearColorValue Settings::getClearColor() { return _clearColor; }

int Settings::getAnisotropicSamples() { return _anisotropicSamples; }

int Settings::getDesiredFPS() { return _desiredFPS; }