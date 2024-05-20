#pragma once
#include <tuple>
#include "Window.h"
#include <vector>
#include <memory>
#include <any>

class InputSubscriber {
 public:
  virtual void cursorNotify(std::any window, float xPos, float yPos) = 0;
  virtual void mouseNotify(std::any window, int button, int action, int mods) = 0;
  virtual void keyNotify(std::any window, int key, int scancode, int action, int mods) = 0;
  virtual void charNotify(std::any window, unsigned int code) = 0;
  virtual void scrollNotify(std::any window, double xOffset, double yOffset) = 0;
};

class Input {
 private:
  std::shared_ptr<Window> _window;
  std::vector<std::shared_ptr<InputSubscriber>> _subscribers;

 public:
  Input(std::shared_ptr<Window> window);
  void cursorHandler(std::any window, double xpos, double ypos);
  void mouseHandler(std::any window, int button, int action, int mods);
  void keyHandler(std::any window, int key, int scancode, int action, int mods);
  void charHandler(std::any window, unsigned int code);
  void scrollHandler(std::any window, double xOffset, double yOffset);
  void subscribe(std::shared_ptr<InputSubscriber> sub);
};