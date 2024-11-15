#pragma once
#include "Vulkan/Window.h"
#include <tuple>
#include <vector>
#include <memory>
#include <any>

class InputSubscriber {
 public:
  virtual void cursorNotify(float xPos, float yPos) = 0;
  virtual void mouseNotify(int button, int action, int mods) = 0;
  virtual void keyNotify(int key, int scancode, int action, int mods) = 0;
  virtual void charNotify(unsigned int code) = 0;
  virtual void scrollNotify(double xOffset, double yOffset) = 0;
};

class InputSubscriberExclusive {
 public:
  virtual bool cursorNotify(float xPos, float yPos) = 0;
  virtual bool mouseNotify(int button, int action, int mods) = 0;
  virtual bool keyNotify(int key, int scancode, int action, int mods) = 0;
  virtual bool charNotify(unsigned int code) = 0;
  virtual bool scrollNotify(double xOffset, double yOffset) = 0;
};

class Input {
 private:
  std::shared_ptr<Window> _window;
  std::vector<std::shared_ptr<InputSubscriberExclusive>> _subscribersExclusive;
  std::vector<std::shared_ptr<InputSubscriber>> _subscribers;
  bool _showCursor = true;

 public:
  Input(std::shared_ptr<Window> window);
  void cursorHandler(double xpos, double ypos);
  void mouseHandler(int button, int action, int mods);
  void keyHandler(int key, int scancode, int action, int mods);
  void charHandler(unsigned int code);
  void scrollHandler(double xOffset, double yOffset);
  void subscribe(std::shared_ptr<InputSubscriber> sub);
  void subscribe(std::shared_ptr<InputSubscriberExclusive> sub);
  void showCursor(bool show);
};