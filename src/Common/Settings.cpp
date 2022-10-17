#include "Settings.h"

Settings::Settings(std::tuple<int, int> resolution, int maxFramesInFlight) {
  _resolution = resolution;
  _maxFramesInFlight = maxFramesInFlight;
}

const std::tuple<int, int>& Settings::getResolution() { return _resolution; }

int Settings::getMaxFramesInFlight() { return _maxFramesInFlight; }
