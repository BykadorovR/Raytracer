#include "Animation.h"

Animation::Animation(const std::vector<std::shared_ptr<NodeGLTF>>& nodes,
                     const std::vector<std::shared_ptr<SkinGLTF>>& skins,
                     const std::vector<std::shared_ptr<AnimationGLTF>>& animations,
                     std::shared_ptr<State> state) {
  _nodes = nodes;
  _skins = skins;
  _animations = animations;
  _state = state;

  _loggerCPU = std::make_shared<LoggerCPU>();

  _descriptorSetLayoutJoints = std::make_shared<DescriptorSetLayout>(_state->getDevice());
  _descriptorSetLayoutJoints->createJoints();

  // initialize joints matrixes for every skin
  _descriptorSetJoints.resize(_skins.size());
  _ssboJoints.resize(_skins.size());
  for (int i = 0; i < _skins.size(); i++) {
    _descriptorSetJoints[i] = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                              _descriptorSetLayoutJoints, _state->getDescriptorPool(),
                                                              _state->getDevice());
    _ssboJoints[i].resize(_state->getSettings()->getMaxFramesInFlight());
    for (int j = 0; j < _state->getSettings()->getMaxFramesInFlight(); j++) {
      _ssboJoints[i][j] = std::make_shared<Buffer>(
          _skins[i]->inverseBindMatrices.size() * sizeof(glm::mat4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state->getDevice());
    }
    _descriptorSetJoints[i]->createJoints(_ssboJoints[i]);
  }

  // store default descriptor set in 0 index and 0 SSBO
  if (_descriptorSetJoints.size() == 0) {
    auto descriptorSetJoints = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                               _descriptorSetLayoutJoints, _state->getDescriptorPool(),
                                                               _state->getDevice());
    descriptorSetJoints->createJoints({});
    _descriptorSetJoints.push_back(descriptorSetJoints);
  }
}

std::shared_ptr<DescriptorSetLayout> Animation::getDescriptorSetLayoutJoints() { return _descriptorSetLayoutJoints; }

std::vector<std::shared_ptr<DescriptorSet>> Animation::getDescriptorSetJoints() { return _descriptorSetJoints; }

// Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
glm::mat4 Animation::_getNodeMatrix(std::shared_ptr<NodeGLTF> node) {
  glm::mat4 nodeMatrix = node->getLocalMatrix();
  std::shared_ptr<NodeGLTF> currentParent = node->parent;
  while (currentParent) {
    nodeMatrix = currentParent->getLocalMatrix() * nodeMatrix;
    currentParent = currentParent->parent;
  }
  return nodeMatrix;
}

void Animation::_updateJoints(int frame, std::shared_ptr<NodeGLTF> node) {
  if (node->skin > -1) {
    // Update the joint matrices
    glm::mat4 inverseTransform = glm::inverse(_getNodeMatrix(node));
    std::shared_ptr<SkinGLTF> skin = _skins[node->skin];
    std::vector<glm::mat4> jointMatrices(skin->joints.size());
    for (size_t i = 0; i < jointMatrices.size(); i++) {
      jointMatrices[i] = _getNodeMatrix(skin->joints[i]) * skin->inverseBindMatrices[i];
      jointMatrices[i] = inverseTransform * jointMatrices[i];
    }

    _ssboJoints[node->skin][frame]->map();
    memcpy(_ssboJoints[node->skin][frame]->getMappedMemory(), jointMatrices.data(),
           jointMatrices.size() * sizeof(glm::mat4));
    _ssboJoints[node->skin][frame]->unmap();
  }

  for (auto& child : node->children) {
    _updateJoints(frame, child);
  }
}

std::vector<std::string> Animation::getAnimations() {
  std::vector<std::string> names(_animations.size());
  for (int i = 0; i < _animations.size(); i++) {
    names[i] = _animations[i]->name;
  }
  return names;
}

void Animation::setAnimation(std::string name) {
  for (int i = 0; i < _animations.size(); i++) {
    if (_animations[i]->name == name) _animationIndex = i;
  }
}

void Animation::updateAnimation(int frame, float deltaTime) {
  // TODO: animation index
  if (_animations.size() == 0 || _animationIndex > static_cast<uint32_t>(_animations.size()) - 1) {
    return;
  }

  _loggerCPU->begin("Update translate/scale/rotation");
  std::shared_ptr<AnimationGLTF> animation = _animations[_animationIndex];
  animation->currentTime += deltaTime;
  animation->currentTime = fmod(animation->currentTime, animation->end);
  for (auto& channel : animation->channels) {
    AnimationSamplerGLTF& sampler = animation->samplers[channel.samplerIndex];
    if (sampler.interpolation != "LINEAR") {
      std::cout << "This sample only supports linear interpolations\n";
      continue;
    }
    if (sampler.inputs.size() <= 1) continue;
    auto higher = std::find_if(sampler.inputs.begin(), sampler.inputs.end(),
                               [current = animation->currentTime](float value) { return value > current; });
    // Get the input keyframe values for the current time stamp
    if (higher != sampler.inputs.begin()) {
      if (higher == sampler.inputs.end() && animation->currentTime > sampler.inputs.back()) continue;
      int right = std::distance(sampler.inputs.begin(), higher);
      int left = right - 1;
      float a = (animation->currentTime - sampler.inputs[left]) / (sampler.inputs[right] - sampler.inputs[left]);
      if (channel.path == "translation") {
        channel.node->translation = glm::mix(sampler.outputsVec4[left], sampler.outputsVec4[right], a);
      }
      if (channel.path == "rotation") {
        glm::quat q1;
        q1.x = sampler.outputsVec4[left].x;
        q1.y = sampler.outputsVec4[left].y;
        q1.z = sampler.outputsVec4[left].z;
        q1.w = sampler.outputsVec4[left].w;

        glm::quat q2;
        q2.x = sampler.outputsVec4[right].x;
        q2.y = sampler.outputsVec4[right].y;
        q2.z = sampler.outputsVec4[right].z;
        q2.w = sampler.outputsVec4[right].w;

        channel.node->rotation = glm::normalize(glm::slerp(q1, q2, a));
      }
      if (channel.path == "scale") {
        channel.node->scale = glm::mix(sampler.outputsVec4[left], sampler.outputsVec4[right], a);
      }
    }
  }
  _loggerCPU->end();

  _loggerCPU->begin("Update matrixes");
  for (auto& node : _nodes) {
    _updateJoints(frame, node);
  }
  _loggerCPU->end();
}