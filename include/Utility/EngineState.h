#pragma once
#include "Vulkan/Window.h"
#include "Vulkan/Instance.h"
#include "Vulkan/Surface.h"
#include "Vulkan/Pool.h"
#include "Vulkan/Device.h"
#include "Vulkan/Filesystem.h"
#include "Vulkan/Allocator.h"
#include "Vulkan/Render.h"
#include "Utility/Settings.h"
#include "Utility/Input.h"
#include "Utility/Logger.h"

class EngineState {
 private:
  std::shared_ptr<Settings> _settings;
  std::shared_ptr<Window> _window;
  std::shared_ptr<Input> _input;
  std::shared_ptr<Instance> _instance;
  std::shared_ptr<Surface> _surface;
  std::shared_ptr<Device> _device;
  std::shared_ptr<DescriptorPool> _descriptorPool;
  std::shared_ptr<Filesystem> _filesystem;
  std::shared_ptr<MemoryAllocator> _memoryAllocator;
  std::shared_ptr<RenderPassManager> _renderPassManager;
  std::shared_ptr<Logger> _logger;
  std::shared_ptr<DebugUtils> _debugUtils;
  int _frameInFlight = 0;
#ifdef __ANDROID__
  AAssetManager* _assetManager;
  ANativeWindow* _nativeWindow;
#endif
 public:
  EngineState(std::shared_ptr<Settings> settings);
#ifdef __ANDROID__
  void setNativeWindow(ANativeWindow* window);
  void setAssetManager(AAssetManager* assetManager);
  AAssetManager* getAssetManager();
#endif
  void initialize();
  std::shared_ptr<Settings> getSettings();
  std::shared_ptr<Window> getWindow();
  std::shared_ptr<Input> getInput();
  std::shared_ptr<MemoryAllocator> getMemoryAllocator();
  std::shared_ptr<Instance> getInstance();
  std::shared_ptr<Surface> getSurface();
  std::shared_ptr<Device> getDevice();
  std::shared_ptr<DescriptorPool> getDescriptorPool();
  std::shared_ptr<Filesystem> getFilesystem();
  std::shared_ptr<RenderPassManager> getRenderPassManager();
  std::shared_ptr<Logger> getLogger();
  std::shared_ptr<DebugUtils> getDebugUtils();
  void setFrameInFlight(int frameInFlight);
  int getFrameInFlight();
};