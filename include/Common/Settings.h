#pragma once
#include <tuple>

struct Settings {
 private:
  int _maxFramesInFlight;
  std::tuple<int, int> _resolution;

 public:
  Settings(std::tuple<int, int> resolution, int maxFramesInFlight);
  const std::tuple<int, int>& getResolution();
  int getMaxFramesInFlight();
};
