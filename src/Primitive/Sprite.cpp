#include "Sprite.h"

Sprite::Sprite(std::shared_ptr<Mesh2D> mesh,
               std::shared_ptr<CommandBuffer> commandBufferTransfer,
               std::shared_ptr<ResourceManager> resourceManager,
               std::shared_ptr<State> state) {
  _state = state;
  _mesh = mesh;

  // default material if model doesn't have material at all, we still have to send data to shader
  _defaultMaterialPhong = std::make_shared<MaterialPhong>(commandBufferTransfer, state);
  _defaultMaterialPhong->setBaseColor(resourceManager->getTextureOne());
  _defaultMaterialPhong->setNormal(resourceManager->getTextureZero());
  _defaultMaterialPhong->setSpecular(resourceManager->getTextureZero());

  _defaultMaterialPBR = std::make_shared<MaterialPBR>(commandBufferTransfer, state);
  _defaultMaterialPBR->setBaseColor(resourceManager->getTextureOne());
  _defaultMaterialPBR->setNormal(resourceManager->getTextureZero());
  _defaultMaterialPBR->setMetallicRoughness(resourceManager->getTextureZero());
  _defaultMaterialPBR->setEmissive(resourceManager->getTextureZero());
  _defaultMaterialPBR->setOccluded(resourceManager->getTextureZero());
  _defaultMaterialPBR->setDiffuseIBL(resourceManager->getCubemapZero()->getTexture());
  _defaultMaterialPBR->setSpecularIBL(resourceManager->getCubemapZero()->getTexture(),
                                      resourceManager->getTextureZero());
  _material = _defaultMaterialPhong;

  // initialize camera UBO and descriptor sets for shadow
  // initialize UBO
  for (int i = 0; i < _state->getSettings()->getMaxDirectionalLights(); i++) {
    _cameraUBODepth.push_back({std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(),
                                                               sizeof(BufferMVP), _state->getDevice())});
  }

  for (int i = 0; i < _state->getSettings()->getMaxPointLights(); i++) {
    std::vector<std::shared_ptr<UniformBuffer>> facesBuffer(6);
    for (int j = 0; j < 6; j++) {
      facesBuffer[j] = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(BufferMVP),
                                                       _state->getDevice());
    }
    _cameraUBODepth.push_back(facesBuffer);
  }
  auto cameraDescriptorSetLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
  cameraDescriptorSetLayout->createUniformBuffer();
  {
    for (int i = 0; i < _state->getSettings()->getMaxDirectionalLights(); i++) {
      auto cameraSet = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                       cameraDescriptorSetLayout, _state->getDescriptorPool(),
                                                       _state->getDevice());
      cameraSet->createUniformBuffer(_cameraUBODepth[i][0]);

      _descriptorSetCameraDepth.push_back({cameraSet});
    }

    for (int i = 0; i < _state->getSettings()->getMaxPointLights(); i++) {
      std::vector<std::shared_ptr<DescriptorSet>> facesSet(6);
      for (int j = 0; j < 6; j++) {
        facesSet[j] = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                      cameraDescriptorSetLayout, _state->getDescriptorPool(),
                                                      _state->getDevice());
        facesSet[j]->createUniformBuffer(_cameraUBODepth[i + _state->getSettings()->getMaxDirectionalLights()][j]);
      }
      _descriptorSetCameraDepth.push_back(facesSet);
    }
  }

  // initialize camera UBO and descriptor sets for draw pass
  // initialize UBO
  _cameraUBOFull = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(BufferMVP),
                                                   _state->getDevice());
  {
    auto cameraSet = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                     cameraDescriptorSetLayout, _state->getDescriptorPool(),
                                                     _state->getDevice());
    cameraSet->createUniformBuffer(_cameraUBOFull);
    _descriptorSetCameraFull = cameraSet;
  }

  // texture descriptors are taken from material
}

void Sprite::setMaterial(std::shared_ptr<MaterialPBR> material) {
  _material = material;
  _materialType = MaterialType::PBR;
}

void Sprite::setMaterial(std::shared_ptr<MaterialPhong> material) {
  _material = material;
  _materialType = MaterialType::PHONG;
}

void Sprite::setMaterial() {
  _material = nullptr;
  _materialType = MaterialType::CUSTOM;
}

MaterialType Sprite::getMaterialType() { return _materialType; }

void Sprite::enableDepth(bool enable) { _enableDepth = enable; }

bool Sprite::isDepthEnabled() { return _enableDepth; }

void Sprite::enableShadow(bool enable) { _enableShadow = enable; }

void Sprite::enableLighting(bool enable) { _enableLighting = enable; }

void Sprite::setModel(glm::mat4 model) { _model = model; }

void Sprite::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

void Sprite::setDrawType(DrawType drawType) { _drawType = drawType; }

DrawType Sprite::getDrawType() { return _drawType; }

void Sprite::draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer, std::shared_ptr<Pipeline> pipeline) {
  if (pipeline->getPushConstants().find("fragment") != pipeline->getPushConstants().end()) {
    LightPush pushConstants;
    pushConstants.enableShadow = _enableShadow;
    pushConstants.enableLighting = _enableLighting;
    pushConstants.cameraPosition = _camera->getEye();

    vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LightPush), &pushConstants);
  }

  BufferMVP cameraMVP{};
  cameraMVP.model = _model;
  cameraMVP.view = _camera->getView();
  cameraMVP.projection = _camera->getProjection();

  void* data;
  vkMapMemory(_state->getDevice()->getLogicalDevice(), _cameraUBOFull->getBuffer()[currentFrame]->getMemory(), 0,
              sizeof(cameraMVP), 0, &data);
  memcpy(data, &cameraMVP, sizeof(cameraMVP));
  vkUnmapMemory(_state->getDevice()->getLogicalDevice(), _cameraUBOFull->getBuffer()[currentFrame]->getMemory());

  VkBuffer vertexBuffers[] = {_mesh->getVertexBuffer()->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame], _mesh->getIndexBuffer()->getBuffer()->getData(),
                       0, VK_INDEX_TYPE_UINT32);

  auto pipelineLayout = pipeline->getDescriptorSetLayout();
  auto cameraLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                   [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                     return info.first == std::string("camera");
                                   });
  if (cameraLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->getPipelineLayout(), 0, 1,
                            &_descriptorSetCameraFull->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  auto textureLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                    [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                      return info.first == std::string("texture");
                                    });
  if (textureLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(),
        2, 1, &_material->getDescriptorSetTextures(currentFrame)->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  auto materialCoefficientsLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                                 [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                                   return info.first == std::string("materialCoefficients");
                                                 });
  if (materialCoefficientsLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(),
        5, 1, &_material->getDescriptorSetCoefficients(currentFrame)->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_mesh->getIndexData().size()),
                   1, 0, 0, 0);
}

void Sprite::drawShadow(int currentFrame,
                        std::shared_ptr<CommandBuffer> commandBuffer,
                        std::shared_ptr<Pipeline> pipeline,
                        int lightIndex,
                        glm::mat4 view,
                        glm::mat4 projection,
                        int face) {
  BufferMVP cameraMVP{};
  cameraMVP.model = _model;
  cameraMVP.view = view;
  cameraMVP.projection = projection;

  void* data;
  vkMapMemory(_state->getDevice()->getLogicalDevice(),
              _cameraUBODepth[lightIndex][face]->getBuffer()[currentFrame]->getMemory(), 0, sizeof(cameraMVP), 0,
              &data);
  memcpy(data, &cameraMVP, sizeof(cameraMVP));
  vkUnmapMemory(_state->getDevice()->getLogicalDevice(),
                _cameraUBODepth[lightIndex][face]->getBuffer()[currentFrame]->getMemory());

  VkBuffer vertexBuffers[] = {_mesh->getVertexBuffer()->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame], _mesh->getIndexBuffer()->getBuffer()->getData(),
                       0, VK_INDEX_TYPE_UINT32);

  auto pipelineLayout = pipeline->getDescriptorSetLayout();
  auto cameraLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                   [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                     return info.first == std::string("camera");
                                   });
  if (cameraLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(),
        0, 1, &_descriptorSetCameraDepth[lightIndex][face]->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_mesh->getIndexData().size()),
                   1, 0, 0, 0);
}