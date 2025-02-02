#include "Utility/RenderGraph.h"

GraphPass::GraphPass(GraphPassStage stage) { _stage = stage; }

void GraphPass::addColorTarget(std::string name, std::vector<std::shared_ptr<Image>> images) {
  _colorTargets[name] = images;
}

GraphPassStage GraphPass::getStage() { return _stage; }

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

RenderGraph::RenderGraph(std::shared_ptr<Swapchain> swapchain) { _swapchain = swapchain; }

std::shared_ptr<GraphPass> RenderGraph::getPass(std::string name) {
  if (_passes.find(name) != _passes.end()) {
    return _passes[name];
  }

  return nullptr;
}

std::shared_ptr<GraphPass> RenderGraph::addPass(std::string name, GraphPassStage stage) {
  _passes[name] = std::make_shared<GraphPass>(stage);
  return _passes[name];
}

void RenderGraph::print() {
  for (auto [key, value] : _passes) {
    std::cout << key << ":" << std::endl;
    for (auto [name, resource] : value->getColorTargets()) {
      std::cout << " color target: " << name << std::endl;
    }
    for (auto [name, resource] : value->getDepthTarget()) {
      std::cout << " depth target: " << name << std::endl;
    }
    for (auto [name, resource] : value->getStorageInputs()) {
      std::cout << " storage input: " << name << std::endl;
    }
    for (auto [name, resource] : value->getStorageOutputs()) {
      std::cout << " storage output: " << name << std::endl;
    }
    for (auto [name, resource] : value->getTextureInputs()) {
      std::cout << " texture input: " << name << std::endl;
    }
    for (auto [name, resource] : value->getVertexBufferInputs()) {
      std::cout << " vertex buffer input: " << name << std::endl;
    }
  }
}

void RenderGraph::calculate() {
  auto getDependencies = [&](std::string name) -> std::vector<std::string> {
    std::vector<std::string> dependencies;
    auto value = _passes[name];
    for (auto [name, resource] : value->getStorageInputs()) {
      dependencies.push_back(name);
    }
    for (auto [name, resource] : value->getStorageOutputs()) {
      dependencies.push_back(name);
    }
    for (auto [name, resource] : value->getTextureInputs()) {
      dependencies.push_back(name);
    }
    for (auto [name, resource] : value->getVertexBufferInputs()) {
      dependencies.push_back(name);
    }

    return dependencies;
  };

  auto findColorTarget = [](std::map<std::string, std::shared_ptr<GraphPass>> passes,
                            std::string findName) -> std::string {
    for (auto [key, value] : passes) {
      for (auto [name, resource] : value->getColorTargets()) {
        if (name == findName) return key;
      }
      for (auto [name, resource] : value->getDepthTarget()) {
        if (name == findName) return key;
      }
    }
    return "";
  };

  std::map<std::shared_ptr<GraphPass>, std::vector<std::shared_ptr<GraphPass>>> passes;
  for (auto [key, value] : _passes) {
    bool root = false;
    for (auto [name, resource] : value->getColorTargets()) {
      for (auto& imageView : _swapchain->getImageViews()) {
        if (std::find(resource.begin(), resource.end(), imageView->getImage()) != resource.end()) {
          root = true;
        }
      }
    }

    auto passesBackup = _passes;
    // we should for every root pass run traversal process
    std::function<void(std::string name, std::shared_ptr<GraphPass>)> traverse = [&](std::string name,
                                                                                     std::shared_ptr<GraphPass> node) {
      passesBackup.erase(name);
      std::cout << name << std::endl;
      auto dependencies = getDependencies(name);
      for (auto dependency : dependencies) {
        // we want to find who writes to this texture, so we are looking for color target or depth target
        auto targetName = findColorTarget(passesBackup, dependency);
        if (targetName.empty() == false) {
          traverse(targetName, _passes[targetName]);
        }
      }
    };

    if (root) {
      traverse(key, value);
    }
  }
}
