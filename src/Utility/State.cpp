#include "Utility/State.h"

State::State(std::shared_ptr<Settings> settings) { _settings = settings; }

#ifdef __ANDROID__
void State::setNativeWindow(ANativeWindow* window) { _nativeWindow = window; }
void State::setAssetManager(AAssetManager* assetManager) { _assetManager = assetManager; }
#endif

void State::initialize() {
  _window = std::make_shared<Window>(_settings->getResolution());
#ifdef __ANDROID__
  _window->setNativeWindow(_nativeWindow);
#endif
  _window->initialize();
  _input = std::make_shared<Input>(_window);
  _instance = std::make_shared<Instance>(_settings->getName(), true);
  _surface = std::make_shared<Surface>(_window, _instance);
  _device = std::make_shared<Device>(_surface, _instance);
  _descriptorPool = std::make_shared<DescriptorPool>(3000, _device);
  _filesystem = std::make_shared<Filesystem>();
#ifdef __ANDROID__
  _filesystem->setAssetManager(_assetManager);
#endif
}

std::shared_ptr<Settings> State::getSettings() { return _settings; }

std::shared_ptr<Window> State::getWindow() { return _window; }

std::shared_ptr<Input> State::getInput() { return _input; }

std::shared_ptr<Instance> State::getInstance() { return _instance; }

std::shared_ptr<Surface> State::getSurface() { return _surface; }

std::shared_ptr<Device> State::getDevice() { return _device; }

std::shared_ptr<DescriptorPool> State::getDescriptorPool() { return _descriptorPool; }

std::shared_ptr<Filesystem> State::getFilesystem() { return _filesystem; }

void State::setFrameInFlight(int frameInFlight) { _frameInFlight = frameInFlight; }

int State::getFrameInFlight() { return _frameInFlight; }