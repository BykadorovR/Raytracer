#pragma once
#include "Loader.h"
#include "Logger.h"
#include "State.h"

class Animation {
 private:
  std::vector<SkinGLTF> _skins;
  std::vector<AnimationGLTF> _animations;
  std::vector<NodeGLTF*> _nodes;
  std::shared_ptr<LoggerCPU> _loggerCPU;
  std::shared_ptr<State> _state;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayoutJoints;
  // separate descriptor for each skin
  std::vector<std::shared_ptr<DescriptorSet>> _descriptorSetJoints;
  std::vector<std::vector<std::shared_ptr<Buffer>>> _ssboJoints;

  void _updateJoints(int frame, NodeGLTF* node);
  glm::mat4 _getNodeMatrix(NodeGLTF* node);

 public:
  Animation(std::vector<NodeGLTF*> nodes,
            std::vector<SkinGLTF> skins,
            std::vector<AnimationGLTF> animations,
            std::shared_ptr<State> state);
  void updateAnimation(int frame, float deltaTime);

  std::shared_ptr<DescriptorSetLayout> getDescriptorSetLayoutJoints();
  std::vector<std::shared_ptr<DescriptorSet>> getDescriptorSetJoints();
};