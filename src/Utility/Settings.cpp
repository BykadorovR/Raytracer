#include "Settings.h"

Settings::Settings(std::string name, std::tuple<int, int> resolution, int maxFramesInFlight) {
  _resolution = resolution;
  _maxFramesInFlight = maxFramesInFlight;
  _name = name;
}

std::string Settings::getName() { return _name; }

const std::tuple<int, int>& Settings::getResolution() { return _resolution; }

int Settings::getMaxFramesInFlight() { return _maxFramesInFlight; }
