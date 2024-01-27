#pragma once
#include <windows.h>
#undef near
#undef far
#include "Core.h"

class Main {
 private:
  std::shared_ptr<Core> _core;
  std::shared_ptr<CameraFly> _camera;

 public:
  Main();
  void update();
  void reset(int width, int height);
  void start();
};