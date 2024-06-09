#pragma once
#include "Loader.h"
#include "Logger.h"
#include "State.h"

class Animation {
 private:
  std::vector<std::shared_ptr<SkinGLTF>> _skins;
  std::vector<std::shared_ptr<AnimationGLTF>> _animations;
  std::vector<std::shared_ptr<NodeGLTF>> _nodes;
  std::shared_ptr<LoggerCPU> _loggerCPU;
  std::shared_ptr<State> _state;
  // separate descriptor for each skin
  std::vector<std::vector<std::shared_ptr<Buffer>>> _ssboJoints;
  int _animationIndex = 0;
  std::map<int, glm::mat4> _matricesJoint;
  bool _play = true;
  std::mutex _mutex;

  void _updateJoints(int currentImage, std::shared_ptr<NodeGLTF> node);
  void _fillMatricesJoint(std::shared_ptr<NodeGLTF> node, glm::mat4 matrixParent);

 public:
  Animation(const std::vector<std::shared_ptr<NodeGLTF>>& nodes,
            const std::vector<std::shared_ptr<SkinGLTF>>& skins,
            const std::vector<std::shared_ptr<AnimationGLTF>>& animations,
            std::shared_ptr<State> state);
  std::vector<std::string> getAnimations();
  void setAnimation(std::string name);
  void setPlay(bool play);
  void setTime(float time);
  std::tuple<float, float> getTimeRange();

  std::tuple<float, float> getTimeline();
  float getCurrentTime();

  void updateAnimation(int currentImage, float deltaTime);

  std::vector<std::vector<std::shared_ptr<Buffer>>> getJointMatricesBuffer();
};