#pragma once
#include <tuple>
#include <string>

struct Settings {
 private:
  int _maxFramesInFlight;
  std::tuple<int, int> _resolution;
  std::string _name;

 public:
  Settings(std::string name, std::tuple<int, int> resolution, int maxFramesInFlight);
  const std::tuple<int, int>& getResolution();
  std::string getName();
  int getMaxFramesInFlight();
};
