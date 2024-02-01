#pragma once
#include <chrono>
#include <thread>
#include "Logger.h"

class TimerFPS {
 private:
  std::chrono::high_resolution_clock::time_point _startTimeCurrent;
  uint64_t _frameCounter;
  double _sumTime;
  int _fps;

  void _reset();

 public:
  TimerFPS();
  void tick();
  void tock();
  int getFPS();
};

class Timer {
 private:
  uint64_t _frameCounter;
  std::chrono::high_resolution_clock::time_point _startTime;
  uint64_t _frameCounterSleep;
  std::chrono::high_resolution_clock::time_point _startTimeCurrent;
  float _elapsedCurrent;
  int _FPSMaxPrevious;

 public:
  Timer();
  void tick();
  void tock();
  void reset();
  void sleep(int FPSMax, std::shared_ptr<LoggerCPU> loggerCPU);
  float getElapsedCurrent();
  uint64_t getFrameCounter();
};