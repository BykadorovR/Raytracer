#include "Graphic/Blur.h"
#include <numbers>

void Blur::_updateWeights() {
  _blurWeights.clear();
  float expectedValue = 0;
  float sum = 0;
  for (int i = -_kernelSize / 2; i <= _kernelSize / 2; i++) {
    float value = std::exp(-pow(i, 2) / (2 * pow(_sigma, 2)));
    _blurWeights.push_back(value);
    sum += value;
  }

  for (auto& coeff : _blurWeights) coeff /= sum;
}

int Blur::getKernelSize() { return _kernelSize; }

int Blur::getSigma() { return _sigma; }

void Blur::setKernelSize(int kernelSize) {
  kernelSize = std::min(kernelSize, 65);
  _kernelSize = kernelSize;
  _updateWeights();
  for (int i = 0; i < _changed.size(); i++) _changed[i] = true;
}

void Blur::setSigma(int sigma) {
  _sigma = sigma;
  _updateWeights();
  for (int i = 0; i < _changed.size(); i++) _changed[i] = true;
}

void Blur::reset(std::vector<std::shared_ptr<Texture>> src, std::vector<std::shared_ptr<Texture>> dst) {
  _initialize(src, dst);
}

void BlurCompute::_updateDescriptors(int currentFrame) {
  _blurWeightsSSBO[currentFrame] = std::make_shared<Buffer>(
      _blurWeights.size() * sizeof(float), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);
  _blurWeightsSSBO[currentFrame]->setData(_blurWeights.data());
}

void BlurCompute::_setWeights(int currentFrame) {
  _descriptorSetWeights->createShaderStorageBuffer(currentFrame, {0}, {_blurWeightsSSBO});
}

void BlurCompute::_initialize(std::vector<std::shared_ptr<Texture>> src, std::vector<std::shared_ptr<Texture>> dst) {
  _descriptorSetHorizontal = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                             _textureLayout, _engineState->getDescriptorPool(),
                                                             _engineState->getDevice());
  _descriptorSetHorizontal->createBlur(src, dst);

  _descriptorSetVertical = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                           _textureLayout, _engineState->getDescriptorPool(),
                                                           _engineState->getDevice());
  _descriptorSetVertical->createBlur(dst, src);
}

BlurCompute::BlurCompute(std::vector<std::shared_ptr<Texture>> src,
                         std::vector<std::shared_ptr<Texture>> dst,
                         std::shared_ptr<EngineState> engineState) {
  _engineState = engineState;

  auto shaderVertical = std::make_shared<Shader>(_engineState);
  shaderVertical->add("shaders/postprocessing/blurVertical_compute.spv", VK_SHADER_STAGE_COMPUTE_BIT);
  auto shaderHorizontal = std::make_shared<Shader>(_engineState);
  shaderHorizontal->add("shaders/postprocessing/blurHorizontal_compute.spv", VK_SHADER_STAGE_COMPUTE_BIT);

  _textureLayout = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
  _textureLayout->createPostprocessing();

  _blurWeightsSSBO.resize(_engineState->getSettings()->getMaxFramesInFlight());
  _changed.resize(_engineState->getSettings()->getMaxFramesInFlight());

  auto bufferLayout = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
  bufferLayout->createShaderStorageBuffer({0}, {VK_SHADER_STAGE_COMPUTE_BIT});
  _descriptorSetWeights = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                          bufferLayout, _engineState->getDescriptorPool(),
                                                          _engineState->getDevice());

  _initialize(src, dst);

  _computePipelineVertical = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());
  _computePipelineVertical->createParticleSystemCompute(
      shaderVertical->getShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT),
      {std::pair{std::string("texture"), _textureLayout}, std::pair{std::string("weights"), bufferLayout}}, {});
  _computePipelineHorizontal = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());
  _computePipelineHorizontal->createParticleSystemCompute(
      shaderHorizontal->getShaderStageInfo(VK_SHADER_STAGE_COMPUTE_BIT),
      {std::pair{std::string("texture"), _textureLayout}, std::pair{std::string("weights"), bufferLayout}}, {});

  _updateWeights();
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _updateDescriptors(i);
    _setWeights(i);
    _changed[i] = false;
  }
}

void BlurCompute::draw(bool horizontal, std::shared_ptr<CommandBuffer> commandBuffer) {
  auto currentFrame = _engineState->getFrameInFlight();
  std::shared_ptr<Pipeline> pipeline = _computePipelineVertical;
  std::shared_ptr<DescriptorSet> descriptorSet = _descriptorSetVertical;
  float groupCountX = 1.f;
  float groupCountY = 128.f;
  if (horizontal) {
    pipeline = _computePipelineHorizontal;
    descriptorSet = _descriptorSetHorizontal;
    groupCountX = 128.f;
    groupCountY = 1.f;
  }
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
                    pipeline->getPipeline());

  if (_changed[currentFrame]) {
    _updateDescriptors(currentFrame);
    _setWeights(currentFrame);
    _changed[currentFrame] = false;
  }

  auto pipelineLayout = pipeline->getDescriptorSetLayout();
  auto computeLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                    [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                      return info.first == std::string("texture");
                                    });
  if (computeLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipeline->getPipelineLayout(), 0, 1, &descriptorSet->getDescriptorSets()[currentFrame], 0,
                            nullptr);
  }

  auto weightsLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                    [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                      return info.first == std::string("weights");
                                    });
  if (weightsLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipeline->getPipelineLayout(), 1, 1,
                            &_descriptorSetWeights->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  auto [width, height] = _engineState->getSettings()->getResolution();
  vkCmdDispatch(commandBuffer->getCommandBuffer()[currentFrame], std::max(1, (int)std::ceil(width / groupCountX)),
                std::max(1, (int)std::ceil(height / groupCountY)), 1);
}

void BlurGraphic::_initialize(std::vector<std::shared_ptr<Texture>> src, std::vector<std::shared_ptr<Texture>> dst) {
  _descriptorSetHorizontal = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                             _layoutBlur, _engineState->getDescriptorPool(),
                                                             _engineState->getDevice());
  _descriptorSetVertical = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                           _layoutBlur, _engineState->getDescriptorPool(),
                                                           _engineState->getDevice());
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
    // write for binding = 1 for textures
    std::vector<VkDescriptorImageInfo> bufferInfoTexture(1);
    bufferInfoTexture[0].imageLayout = src[i]->getImageView()->getImage()->getImageLayout();
    bufferInfoTexture[0].imageView = src[i]->getImageView()->getImageView();
    bufferInfoTexture[0].sampler = src[i]->getSampler()->getSampler();
    textureInfoColor[0] = bufferInfoTexture;

    std::vector<VkDescriptorBufferInfo> bufferInfoCoeff(1);
    // write to binding = 0 for vertex shader
    bufferInfoCoeff[0].buffer = _blurWeightsSSBO[i]->getData();
    bufferInfoCoeff[0].offset = 0;
    bufferInfoCoeff[0].range = _blurWeightsSSBO[i]->getSize();
    bufferInfoColor[1] = bufferInfoCoeff;

    _descriptorSetHorizontal->createCustom(i, bufferInfoColor, textureInfoColor);

    textureInfoColor.clear();
    // write for binding = 1 for textures
    bufferInfoTexture = std::vector<VkDescriptorImageInfo>(1);
    bufferInfoTexture[0].imageLayout = dst[i]->getImageView()->getImage()->getImageLayout();
    bufferInfoTexture[0].imageView = dst[i]->getImageView()->getImageView();
    bufferInfoTexture[0].sampler = dst[i]->getSampler()->getSampler();
    textureInfoColor[0] = bufferInfoTexture;

    _descriptorSetVertical->createCustom(i, bufferInfoColor, textureInfoColor);
  }
}

void BlurGraphic::_updateDescriptors(int currentFrame) {
  _blurWeightsSSBO[currentFrame] = std::make_shared<Buffer>(
      _blurWeights.size() * sizeof(float), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);
  _blurWeightsSSBO[currentFrame]->setData(_blurWeights.data());
}

void BlurGraphic::_setWeights(int currentFrame) {
  std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
  std::vector<VkDescriptorBufferInfo> bufferInfoCoeff(1);
  // write to binding = 0 for vertex shader
  bufferInfoCoeff[0].buffer = _blurWeightsSSBO[currentFrame]->getData();
  bufferInfoCoeff[0].offset = 0;
  bufferInfoCoeff[0].range = _blurWeightsSSBO[currentFrame]->getSize();
  bufferInfoColor[1] = bufferInfoCoeff;

  _descriptorSetHorizontal->createCustom(currentFrame, bufferInfoColor, {});
  _descriptorSetVertical->createCustom(currentFrame, bufferInfoColor, {});
}

BlurGraphic::BlurGraphic(std::vector<std::shared_ptr<Texture>> src,
                         std::vector<std::shared_ptr<Texture>> dst,
                         std::shared_ptr<CommandBuffer> commandBufferTransfer,
                         std::shared_ptr<EngineState> engineState) {
  _engineState = engineState;
  _resolution = src[0]->getImageView()->getImage()->getResolution();

  auto shaderVertical = std::make_shared<Shader>(_engineState);
  shaderVertical->add("shaders/postprocessing/blur_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shaderVertical->add("shaders/postprocessing/blurVertical_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  auto shaderHorizontal = std::make_shared<Shader>(_engineState);
  shaderHorizontal->add("shaders/postprocessing/blur_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shaderHorizontal->add("shaders/postprocessing/blurHorizontal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  _blurWeightsSSBO.resize(_engineState->getSettings()->getMaxFramesInFlight());
  _changed.resize(_engineState->getSettings()->getMaxFramesInFlight());
  _mesh = std::make_shared<MeshStatic2D>(engineState);
  // 3   0
  // 2   1
  _mesh->setVertices({Vertex2D{{1.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}, {1.f, 0.f, 0.f, 1.f}},
                      Vertex2D{{1.f, -1.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}, {1.f, 0.f, 0.f, 1.f}},
                      Vertex2D{{-1.f, -1.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}, {1.f, 0.f, 0.f, 1.f}},
                      Vertex2D{{-1.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}, {1.f, 0.f, 0.f, 1.f}}},
                     commandBufferTransfer);
  _mesh->setIndexes({0, 3, 2, 2, 1, 0}, commandBufferTransfer);

  _renderPass = std::make_shared<RenderPass>(_engineState->getSettings(), _engineState->getDevice());
  _renderPass->initializeBlur();

  _updateWeights();
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
    _updateDescriptors(i);
    _changed[i] = false;
  }

  _layoutBlur = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
  std::vector<VkDescriptorSetLayoutBinding> layoutBinding(2);
  layoutBinding[0].binding = 0;
  layoutBinding[0].descriptorCount = 1;
  layoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutBinding[0].pImmutableSamplers = nullptr;
  layoutBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  layoutBinding[1].binding = 1;
  layoutBinding[1].descriptorCount = 1;
  layoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  layoutBinding[1].pImmutableSamplers = nullptr;
  layoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  _layoutBlur->createCustom(layoutBinding);

  _initialize(src, dst);

  _computePipelineHorizontal = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());
  _computePipelineHorizontal->createGraphic3D(
      VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
      {shaderHorizontal->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
       shaderHorizontal->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
      {std::pair{std::string("blur"), _layoutBlur}}, {}, _mesh->getBindingDescription(),
      _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex2D, pos)},
                                             {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex2D, texCoord)}}),
      _renderPass);

  _computePipelineVertical = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());
  _computePipelineVertical->createGraphic3D(
      VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
      {shaderVertical->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
       shaderVertical->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
      {std::pair{std::string("blur"), _layoutBlur}}, {}, _mesh->getBindingDescription(),
      _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex2D, pos)},
                                             {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex2D, texCoord)}}),
      _renderPass);
}

void BlurGraphic::draw(bool horizontal, std::shared_ptr<CommandBuffer> commandBuffer) {
  int currentFrame = _engineState->getFrameInFlight();
  std::shared_ptr<Pipeline> pipeline = _computePipelineVertical;
  std::shared_ptr<DescriptorSet> descriptorSet = _descriptorSetVertical;
  if (horizontal) {
    pipeline = _computePipelineHorizontal;
    descriptorSet = _descriptorSetHorizontal;
  }
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->getPipeline());

  if (_changed[currentFrame]) {
    _updateDescriptors(currentFrame);
    _setWeights(currentFrame);
    _changed[currentFrame] = false;
  }

  auto resolution = _resolution;
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = std::get<1>(resolution);
  viewport.width = std::get<0>(resolution);
  viewport.height = -std::get<1>(resolution);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(std::get<0>(resolution), std::get<1>(resolution));
  vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  VkBuffer vertexBuffers[] = {_mesh->getVertexBuffer()->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame], _mesh->getIndexBuffer()->getBuffer()->getData(),
                       0, VK_INDEX_TYPE_UINT32);

  auto pipelineLayout = pipeline->getDescriptorSetLayout();
  vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline->getPipelineLayout(), 0, 1, &descriptorSet->getDescriptorSets()[currentFrame], 0,
                          nullptr);

  vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_mesh->getIndexData().size()),
                   1, 0, 0, 0);
}