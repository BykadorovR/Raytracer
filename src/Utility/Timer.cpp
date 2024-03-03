module Timer;

namespace VulkanEngine {
TimerFPS::TimerFPS() {
  _fps = 0;
  _reset();
}

void TimerFPS::_reset() {
  _frameCounter = 0;
  _sumTime = 0;
}

void TimerFPS::tick() { _startTimeCurrent = std::chrono::high_resolution_clock::now(); }

void TimerFPS::tock() {
  _frameCounter++;
  auto end = std::chrono::high_resolution_clock::now();

  // calculate frames per second
  std::chrono::duration<double> elapsed = end - _startTimeCurrent;
  _sumTime += elapsed.count();
  if (_sumTime > 1.f) {
    _fps = _frameCounter;
    _reset();
  }
}

int TimerFPS::getFPS() { return _fps; }

Timer::Timer() {
  _frameCounter = 0;
  _frameCounterSleep = 0;
  _elapsedCurrent = 0;
  _FPSMaxPrevious = 0;
  _startTime = std::chrono::high_resolution_clock::now();
}

void Timer::reset() {
  _frameCounterSleep = 0;
  _startTime = std::chrono::high_resolution_clock::now();
}

void Timer::tick() { _startTimeCurrent = std::chrono::high_resolution_clock::now(); }

void Timer::tock() {
  _frameCounter++;
  _frameCounterSleep++;
  auto end = std::chrono::high_resolution_clock::now();

  // calculate current timing for models and particle systems
  std::chrono::duration<double> elapsedCurrent = (end - _startTimeCurrent);
  _elapsedCurrent = elapsedCurrent.count();
}

float Timer::getElapsedCurrent() { return _elapsedCurrent; }

void Timer::sleep(int FPSMax, std::shared_ptr<VulkanEngine::LoggerCPU> loggerCPU) {
  // have to reset timer if FPS has been changed
  if (_FPSMaxPrevious && _FPSMaxPrevious != FPSMax) reset();
  auto end = std::chrono::high_resolution_clock::now();
  _FPSMaxPrevious = FPSMax;
  auto elapsedSleep = std::chrono::duration_cast<std::chrono::milliseconds>(end - _startTime).count();
  uint64_t expected = (1000.f / FPSMax) * _frameCounterSleep;
  if (elapsedSleep < expected) {
    loggerCPU->begin("Sleep for: " + std::to_string(expected - elapsedSleep));
    std::this_thread::sleep_for(std::chrono::milliseconds(expected - elapsedSleep));
    loggerCPU->end();
  }
}

uint64_t Timer::getFrameCounter() { return _frameCounter; }
}  // namespace VulkanEngine