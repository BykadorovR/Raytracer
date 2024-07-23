#pragma once
#include <string>

class Named {
 private:
  std::string _name;

 public:
  void setName(std::string name);
  std::string getName();
};