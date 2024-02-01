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
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayoutJoints;
  // separate descriptor for each skin
  std::vector<std::shared_ptr<DescriptorSet>> _descriptorSetJoints;
  std::vector<std::vector<std::shared_ptr<Buffer>>> _ssboJoints;
  int _animationIndex = 0;
  std::map<int, glm::mat4> _matricesJoint;
  void _updateJoints(int frame, std::shared_ptr<NodeGLTF> node);
  void _fillMatricesJoint(std::shared_ptr<NodeGLTF> node, glm::mat4 matrixParent);

 public:
  Animation(const std::vector<std::shared_ptr<NodeGLTF>>& nodes,
            const std::vector<std::shared_ptr<SkinGLTF>>& skins,
            const std::vector<std::shared_ptr<AnimationGLTF>>& animations,
            std::shared_ptr<State> state);
  std::vector<std::string> getAnimations();
  void setAnimation(std::string name);

  void updateAnimation(int frame, float deltaTime);

  std::shared_ptr<DescriptorSetLayout> getDescriptorSetLayoutJoints();
  std::vector<std::shared_ptr<DescriptorSet>> getDescriptorSetJoints();
};