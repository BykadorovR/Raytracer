#include "State.h"

State::State(std::shared_ptr<Window> window, std::shared_ptr<Settings> settings) {
  _settings = settings;
  _window = window;
  _input = std::make_shared<Input>(_window);
  _instance = std::make_shared<Instance>(settings->getName(), true);
  _surface = std::make_shared<Surface>(_window, _instance);
  _device = std::make_shared<Device>(_surface, _instance);
  _descriptorPool = std::make_shared<DescriptorPool>(3000, _device);
}

std::shared_ptr<Settings> State::getSettings() { return _settings; }

std::shared_ptr<Window> State::getWindow() { return _window; }

std::shared_ptr<Input> State::getInput() { return _input; }

std::shared_ptr<Instance> State::getInstance() { return _instance; }

std::shared_ptr<Surface> State::getSurface() { return _surface; }

std::shared_ptr<Device> State::getDevice() { return _device; }

std::shared_ptr<DescriptorPool> State::getDescriptorPool() { return _descriptorPool; }

void State::setFrameInFlight(int frameInFlight) { _frameInFlight = frameInFlight; }

int State::getFrameInFlight() { return _frameInFlight; }