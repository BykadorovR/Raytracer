#include "Utility/Animation.h"

Animation::Animation(const std::vector<std::shared_ptr<NodeGLTF>>& nodes,
                     const std::vector<std::shared_ptr<SkinGLTF>>& skins,
                     const std::vector<std::shared_ptr<AnimationGLTF>>& animations,
                     std::shared_ptr<State> state) {
  _nodes = nodes;
  _skins = skins;
  _animations = animations;
  _state = state;

  _logger = std::make_shared<Logger>();

  _ssboJoints.resize(_skins.size());
  for (int i = 0; i < _skins.size(); i++) {
    _ssboJoints[i].resize(_state->getSettings()->getMaxFramesInFlight());
    for (int j = 0; j < _state->getSettings()->getMaxFramesInFlight(); j++) {
      _ssboJoints[i][j] = std::make_shared<Buffer>(
          sizeof(glm::vec4) + _skins[i]->inverseBindMatrices.size() * sizeof(glm::mat4),
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state);
    }
  }

  // store default descriptor set in 0 index and 0 SSBO
  if (_skins.size() == 0) {
    std::vector<std::shared_ptr<Buffer>> bufferStubs;
    for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
      bufferStubs.push_back(
          std::make_shared<Buffer>(sizeof(glm::mat4) + sizeof(glm::vec4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _state));
    }
    _ssboJoints.push_back(bufferStubs);
    auto identityMat = glm::mat4(1.f);
    int jointNumber = 0;
    for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
      _ssboJoints[0][i]->map();
      memcpy(_ssboJoints[0][i]->getMappedMemory(), &jointNumber, sizeof(glm::vec4));
      memcpy((uint8_t*)(_ssboJoints[0][i]->getMappedMemory()) + sizeof(glm::vec4), &identityMat, sizeof(glm::mat4));
      _ssboJoints[0][i]->unmap();
    }
  }
}

std::vector<std::vector<std::shared_ptr<Buffer>>> Animation::getJointMatricesBuffer() { return _ssboJoints; }

void Animation::_updateJoints(int currentImage, std::shared_ptr<NodeGLTF> node) {
  if (node->skin > -1) {
    // Update the joint matrices
    // glm::mat4 inverseTransform = glm::inverse(_getNodeMatrix(node));
    glm::mat4 inverseTransform = glm::inverse(_matricesJoint[node->index]);
    std::shared_ptr<SkinGLTF> skin = _skins[node->skin];
    std::vector<glm::mat4> jointMatrices(skin->joints.size());
    for (size_t i = 0; i < jointMatrices.size(); i++) {
      // jointMatrices[i] = _getNodeMatrix(skin->joints[i]) * skin->inverseBindMatrices[i];
      jointMatrices[i] = _matricesJoint[skin->joints[i]->index] * skin->inverseBindMatrices[i];
      jointMatrices[i] = inverseTransform * jointMatrices[i];
    }
    auto frameInFlight = currentImage % _state->getSettings()->getMaxFramesInFlight();
    int jointNumber = jointMatrices.size();
    _ssboJoints[node->skin][frameInFlight]->map();
    memcpy(_ssboJoints[node->skin][frameInFlight]->getMappedMemory(), &jointNumber, sizeof(glm::vec4));
    memcpy((uint8_t*)(_ssboJoints[node->skin][frameInFlight]->getMappedMemory()) + sizeof(glm::vec4),
           jointMatrices.data(), jointMatrices.size() * sizeof(glm::mat4));
    _ssboJoints[node->skin][frameInFlight]->unmap();
  }

  for (auto& child : node->children) {
    _updateJoints(currentImage, child);
  }
}

void Animation::_fillMatricesJoint(std::shared_ptr<NodeGLTF> node, glm::mat4 matrixParent) {
  _matricesJoint[node->index] = matrixParent * node->getLocalMatrix();
  for (auto& child : node->children) {
    _fillMatricesJoint(child, _matricesJoint[node->index]);
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
  std::unique_lock<std::mutex> lock(_mutex);
  for (int i = 0; i < _animations.size(); i++) {
    if (_animations[i]->name == name) _animationIndex = i;
  }
}

void Animation::setPlay(bool play) {
  std::unique_lock<std::mutex> lock(_mutex);
  _play = play;
}

std::tuple<float, float> Animation::getTimeline() {
  return {_animations[_animationIndex]->start, _animations[_animationIndex]->end};
}

void Animation::setTime(float time) {
  std::unique_lock<std::mutex> lock(_mutex);
  _animations[_animationIndex]->currentTime = time;
}

std::tuple<float, float> Animation::getTimeRange() {
  return {0, _animations[_animationIndex]->end - _animations[_animationIndex]->start};
}

// TODO: mutex?
float Animation::getCurrentTime() { return _animations[_animationIndex]->currentTime; }

void Animation::calculateJoints(float deltaTime) {
  std::unique_lock<std::mutex> lock(_mutex);
  if (_play == false || _animations.size() == 0 || _animationIndex > static_cast<uint32_t>(_animations.size()) - 1) {
    return;
  }

  _logger->begin("Update translate/scale/rotation");
  std::shared_ptr<AnimationGLTF> animation = _animations[_animationIndex];
  animation->currentTime += deltaTime;
  animation->currentTime = fmod(animation->currentTime, animation->end - animation->start);
  for (auto& channel : animation->channels) {
    AnimationSamplerGLTF& sampler = animation->samplers[channel.samplerIndex];
    if (sampler.interpolation != "LINEAR") {
      std::cout << "This sample only supports linear interpolations\n";
      continue;
    }
    if (sampler.inputs.size() <= 1) continue;
    auto higher = std::find_if(sampler.inputs.begin(), sampler.inputs.end(),
                               [current = animation->currentTime, start = animation->start](float value) {
                                 return value > (current + start);
                               });
    // Get the input keyframe values for the current time stamp
    if (higher != sampler.inputs.begin()) {
      if (higher == sampler.inputs.end() && (animation->currentTime + animation->start) > sampler.inputs.back())
        continue;
      int right = std::distance(sampler.inputs.begin(), higher);
      int left = right - 1;
      float a = (animation->currentTime + animation->start - sampler.inputs[left]) /
                (sampler.inputs[right] - sampler.inputs[left]);
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
  _logger->end();

  _logger->begin("Update matrixes");
  _matricesJoint.clear();
  for (auto& node : _nodes) {
    _fillMatricesJoint(node, glm::mat4(1.f));
  }
  _logger->end();
}

void Animation::updateBuffers(int currentImage) {
  for (auto& node : _nodes) {
    _updateJoints(currentImage, node);
  }
}