#include "Utility/RenderGraph.h"

void GraphPass::addColorTarget(std::string name, std::vector<std::shared_ptr<Image>> images) {
  _colorTargets[name] = images;
}

std::map<std::string, std::vector<std::shared_ptr<Image>>> GraphPass::getColorTargets() { return _colorTargets; }

std::map<std::string, std::shared_ptr<Image>> GraphPass::getDepthTarget() { return _depthTarget; }

std::map<std::string, std::vector<std::shared_ptr<Buffer>>> GraphPass::getStorageInputs() { return _storageInputs; }

std::map<std::string, std::vector<std::shared_ptr<Buffer>>> GraphPass::getStorageOutputs() { return _storageOutputs; }

std::map<std::string, std::vector<std::shared_ptr<Buffer>>> GraphPass::getVertexBufferInputs() {
  return _vertexBufferInputs;
}

std::map<std::string, std::vector<std::shared_ptr<Image>>> GraphPass::getTextureInputs() { return _textureInputs; }

void GraphPass::addStorageInput(std::string name, std::vector<std::shared_ptr<Buffer>> buffers) {
  _storageInputs[name] = buffers;
}

void GraphPass::addStorageOutput(std::string name, std::vector<std::shared_ptr<Buffer>> buffers) {
  _storageOutputs[name] = buffers;
}

void GraphPass::addVertexBufferInput(std::string name, std::vector<std::shared_ptr<Buffer>> buffers) {
  _vertexBufferInputs[name] = buffers;
}

void GraphPass::addTextureInput(std::string name, std::vector<std::shared_ptr<Image>> images) {
  _textureInputs[name] = images;
}

void GraphPass::setDepthTarget(std::string name, std::shared_ptr<Image> image) { _depthTarget[name] = image; }

RenderGraph::RenderGraph() {}

std::shared_ptr<GraphPass> RenderGraph::getPass(std::string name) {
  if (_passes.find(name) == _passes.end()) {
    _passes[name] = std::make_shared<GraphPass>();
  }

  return _passes[name];
}

void RenderGraph::calculate() {
  for (auto [key, value] : _passes) {
    std::cout << key << ":";
    for (auto [name, resource] : value->getColorTargets()) {
      std::cout << " color target: " << name << ",";
    }
    for (auto [name, resource] : value->getDepthTarget()) {
      std::cout << " depth target: " << name << ",";
    }
    for (auto [name, resource] : value->getStorageInputs()) {
      std::cout << " storage input: " << name << ",";
    }
    for (auto [name, resource] : value->getStorageOutputs()) {
      std::cout << " storage output: " << name << ",";
    }
    for (auto [name, resource] : value->getTextureInputs()) {
      std::cout << " texture input: " << name << ",";
    }
    for (auto [name, resource] : value->getVertexBufferInputs()) {
      std::cout << " vertex buffer input: " << name << ",";
    }
    std::cout << std::endl;
  }
}
