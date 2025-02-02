#pragma once
#include <string>
#include <map>
#include <vector>
#include <memory>
#include "Vulkan/Image.h"
#include "Vulkan/Swapchain.h"

enum class GraphPassStage { GRAPHIC = 0, COMPUTE = 1, TRANSFER = 2 };

class GraphPass {
 private:
  GraphPassStage _stage;
  // TODO: can contain either frameInFlight or number in swapchain targets, need to handle appropriately
  std::map<std::string, std::vector<std::shared_ptr<Image>>> _colorTargets;
  std::map<std::string, std::shared_ptr<Image>> _depthTarget;
  std::map<std::string, std::vector<std::shared_ptr<Buffer>>> _storageInputs;
  std::map<std::string, std::vector<std::shared_ptr<Buffer>>> _storageOutputs;
  std::map<std::string, std::vector<std::shared_ptr<Buffer>>> _vertexBufferInputs;
  std::map<std::string, std::vector<std::shared_ptr<Image>>> _textureInputs;

 public:
  GraphPass(GraphPassStage stage);
  // handle attachments
  void addColorTarget(std::string name, std::vector<std::shared_ptr<Image>> images);
  void setDepthTarget(std::string name, std::shared_ptr<Image> image);
  // handle input to shaders
  void addTextureInput(std::string name, std::vector<std::shared_ptr<Image>> images);
  void addUniformInput();
  void addStorageInput(std::string name, std::vector<std::shared_ptr<Buffer>> buffers);
  // handle output from shaders
  void addStorageOutput(std::string name, std::vector<std::shared_ptr<Buffer>> buffers);
  // handle vertices
  void addVertexBufferInput(std::string name, std::vector<std::shared_ptr<Buffer>> buffers);
  void addIndexBufferInput();

  GraphPassStage getStage();
  std::map<std::string, std::vector<std::shared_ptr<Image>>> getColorTargets();
  std::map<std::string, std::shared_ptr<Image>> getDepthTarget();
  std::map<std::string, std::vector<std::shared_ptr<Buffer>>> getStorageInputs();
  std::map<std::string, std::vector<std::shared_ptr<Buffer>>> getStorageOutputs();
  std::map<std::string, std::vector<std::shared_ptr<Buffer>>> getVertexBufferInputs();
  std::map<std::string, std::vector<std::shared_ptr<Image>>> getTextureInputs();

  // set function that does render pass work
  void setRenderExecution();
};

class RenderGraph {
 private:
  std::map<std::string, std::shared_ptr<GraphPass>> _passes;
  std ::shared_ptr<Swapchain> _swapchain;

 public:
  RenderGraph(std::shared_ptr<Swapchain> swapchain);
  std::shared_ptr<GraphPass> getPass(std::string name);
  std::shared_ptr<GraphPass> addPass(std::string name, GraphPassStage stage);
  void calculate();
  void render();
  void print();
};