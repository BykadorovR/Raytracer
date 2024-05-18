#include "IBL.h"

IBL::IBL(std::shared_ptr<LightManager> lightManager,
         std::shared_ptr<CommandBuffer> commandBufferTransfer,
         std::shared_ptr<ResourceManager> resourceManager,
         std::shared_ptr<State> state) {
  _state = state;
  _commandBufferTransfer = commandBufferTransfer;
  _mesh3D = std::make_shared<Mesh3D>(state);
  _mesh2D = std::make_shared<Mesh2D>(state);
  _lightManager = lightManager;

  std::vector<Vertex3D> vertices(8);
  vertices[0].pos = glm::vec3(-0.5, -0.5, 0.5);   // 0
  vertices[1].pos = glm::vec3(0.5, -0.5, 0.5);    // 1
  vertices[2].pos = glm::vec3(-0.5, 0.5, 0.5);    // 2
  vertices[3].pos = glm::vec3(0.5, 0.5, 0.5);     // 3
  vertices[4].pos = glm::vec3(-0.5, -0.5, -0.5);  // 4
  vertices[5].pos = glm::vec3(0.5, -0.5, -0.5);   // 5
  vertices[6].pos = glm::vec3(-0.5, 0.5, -0.5);   // 6
  vertices[7].pos = glm::vec3(0.5, 0.5, -0.5);    // 7

  std::vector<uint32_t> indices{                    // Bottom
                                0, 4, 5, 5, 1, 0,   // ccw if look to this face from down
                                                    //  Top
                                2, 3, 7, 7, 6, 2,   // ccw if look to this face from up
                                                    //  Left
                                0, 2, 6, 6, 4, 0,   // ccw if look to this face from left
                                                    //  Right
                                1, 5, 7, 7, 3, 1,   // ccw if look to this face from right
                                                    //  Front
                                1, 3, 2, 2, 0, 1,   // ccw if look to this face from front
                                                    //  Back
                                5, 4, 6, 6, 7, 5};  // ccw if look to this face from back
  _mesh3D->setVertices(vertices, commandBufferTransfer);
  _mesh3D->setIndexes(indices, commandBufferTransfer);
  _mesh3D->setColor(std::vector<glm::vec3>(vertices.size(), glm::vec3(1.f, 1.f, 1.f)), commandBufferTransfer);
  // 3   0
  // 2   1
  _mesh2D->setVertices(
      {Vertex2D{{0.5f, 0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.f, 0.f}},
       Vertex2D{{0.5f, -0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.f, 0.f}},
       Vertex2D{{-0.5f, -0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.f, 0.f}},
       Vertex2D{{-0.5f, 0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.f, 0.f}}},
      commandBufferTransfer);
  _mesh2D->setIndexes({0, 3, 2, 2, 1, 0}, commandBufferTransfer);

  // TODO: should be texture zero??
  _material = std::make_shared<MaterialColor>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
  std::dynamic_pointer_cast<MaterialColor>(_material)->setBaseColor({resourceManager->getCubemapOne()->getTexture()});

  auto cameraLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
  cameraLayout->createUniformBuffer();
  // initialize camera UBO and descriptor sets for draw
  // initialize UBO
  _cameraBufferCubemap.resize(6);
  for (int i = 0; i < 6; i++) {
    _cameraBufferCubemap[i] = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(),
                                                              sizeof(BufferMVP), state);
  }
  _cameraBuffer = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(BufferMVP),
                                                  state);

  _descriptorSetCameraCubemap.resize(6);
  for (int i = 0; i < 6; i++) {
    _descriptorSetCameraCubemap[i] = std::make_shared<DescriptorSet>(
        state->getSettings()->getMaxFramesInFlight(), cameraLayout, state->getDescriptorPool(), state->getDevice());
    _descriptorSetCameraCubemap[i]->createUniformBuffer(_cameraBufferCubemap[i]);
  }
  _descriptorSetCamera = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(), cameraLayout,
                                                         state->getDescriptorPool(), state->getDevice());
  _descriptorSetCamera->createUniformBuffer(_cameraBuffer);

  _renderPass = std::make_shared<RenderPass>(_state->getSettings(), _state->getDevice());
  _renderPass->initializeIBL();
  {
    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add("shaders/skyboxDiffuse_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/skyboxDiffuse_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelineDiffuse = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineDiffuse->createGraphic3D(VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
                                      {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                       shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                      {std::pair{std::string("camera"), cameraLayout},
                                       std::pair{std::string("texture"), _material->getDescriptorSetLayoutTextures()}},
                                      {}, _mesh3D->getBindingDescription(), _mesh3D->getAttributeDescriptions(),
                                      _renderPass);
  }
  {
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["fragment"] = RoughnessConstants::getPushConstant(0);

    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add("shaders/skyboxSpecular_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/skyboxSpecular_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelineSpecular = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineSpecular->createGraphic3D(VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
                                       {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                        shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                       {std::pair{std::string("camera"), cameraLayout},
                                        std::pair{std::string("texture"), _material->getDescriptorSetLayoutTextures()}},
                                       defaultPushConstants, _mesh3D->getBindingDescription(),
                                       _mesh3D->getAttributeDescriptions(), _renderPass);
  }
  {
    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add("shaders/specularBRDF_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/specularBRDF_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelineSpecularBRDF = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineSpecularBRDF->createGraphic2D(VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, true,
                                           {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                            shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                           {{"camera", cameraLayout}}, {}, _mesh2D->getBindingDescription(),
                                           _mesh2D->getAttributeDescriptions(), _renderPass);
  }

  _loggerGPU = std::make_shared<LoggerGPU>(state);

  _cubemapDiffuse = std::make_shared<Cubemap>(
      _state->getSettings()->getDiffuseIBLResolution(), _state->getSettings()->getGraphicColorFormat(), 1,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);
  _cubemapSpecular = std::make_shared<Cubemap>(
      _state->getSettings()->getSpecularIBLResolution(), _state->getSettings()->getGraphicColorFormat(),
      _state->getSettings()->getSpecularMipMap(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);
  auto brdfImage = std::make_shared<Image>(_state->getSettings()->getDepthResolution(), 1, 1,
                                           _state->getSettings()->getGraphicColorFormat(), VK_IMAGE_TILING_OPTIMAL,
                                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state);
  brdfImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, commandBufferTransfer);
  auto brdfImageView = std::make_shared<ImageView>(brdfImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                   VK_IMAGE_ASPECT_COLOR_BIT, state);
  _textureSpecularBRDF = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, brdfImageView, state);

  _cameraSpecularBRDF = std::make_shared<CameraOrtho>();
  _cameraSpecularBRDF->setProjectionParameters({-1, 1, 1, -1}, 0, 1);
  _cameraSpecularBRDF->setViewParameters(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));

  _camera = std::make_shared<CameraFly>(_state->getSettings());
  _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, 1.f, 0.f));
  _camera->setProjectionParameters(90.f, 0.1f, 100.f);
  _camera->setAspect(1.f);

  _frameBufferSpecular.resize(6);
  for (int i = 0; i < 6; i++) {
    _frameBufferSpecular[i].resize(_state->getSettings()->getSpecularMipMap());
    for (int j = 0; j < _state->getSettings()->getSpecularMipMap(); j++) {
      auto currentTexture = _cubemapSpecular->getTextureSeparate()[i][j];
      auto [width, height] = currentTexture->getImageView()->getImage()->getResolution();
      _frameBufferSpecular[i][j] = std::make_shared<Framebuffer>(
          std::vector{currentTexture->getImageView()}, std::tuple{width * std::pow(0.5, j), height * std::pow(0.5, j)},
          _renderPass, _state->getDevice());
    }
  }

  _frameBufferDiffuse.resize(6);
  for (int i = 0; i < 6; i++) {
    auto currentTexture = _cubemapDiffuse->getTextureSeparate()[i][0];
    _frameBufferDiffuse[i] = std::make_shared<Framebuffer>(std::vector{currentTexture->getImageView()},
                                                           currentTexture->getImageView()->getImage()->getResolution(),
                                                           _renderPass, _state->getDevice());
  }

  _frameBufferBRDF = std::make_shared<Framebuffer>(std::vector{_textureSpecularBRDF->getImageView()},
                                                   _textureSpecularBRDF->getImageView()->getImage()->getResolution(),
                                                   _renderPass, _state->getDevice());
}

std::shared_ptr<Cubemap> IBL::getCubemapDiffuse() { return _cubemapDiffuse; }

std::shared_ptr<Cubemap> IBL::getCubemapSpecular() { return _cubemapSpecular; }

std::shared_ptr<Texture> IBL::getTextureSpecularBRDF() { return _textureSpecularBRDF; }

void IBL::setMaterial(std::shared_ptr<MaterialColor> material) { _material = material; }

void IBL::setPosition(glm::vec3 position) {
  _model = glm::translate(glm::mat4(1.f), position);
  _camera->setViewParameters(position, glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, 1.f, 0.f));
}

void IBL::_draw(int face,
                std::shared_ptr<Camera> camera,
                std::shared_ptr<CommandBuffer> commandBuffer,
                std::shared_ptr<Pipeline> pipeline) {
  auto currentFrame = _state->getFrameInFlight();
  BufferMVP cameraUBO{};
  cameraUBO.model = _model;
  cameraUBO.view = camera->getView();
  cameraUBO.projection = camera->getProjection();

  void* data;
  vkMapMemory(_state->getDevice()->getLogicalDevice(),
              _cameraBufferCubemap[face]->getBuffer()[currentFrame]->getMemory(), 0, sizeof(cameraUBO), 0, &data);
  memcpy(data, &cameraUBO, sizeof(cameraUBO));
  vkUnmapMemory(_state->getDevice()->getLogicalDevice(),
                _cameraBufferCubemap[face]->getBuffer()[currentFrame]->getMemory());

  VkBuffer vertexBuffers[] = {_mesh3D->getVertexBuffer()->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame],
                       _mesh3D->getIndexBuffer()->getBuffer()->getData(), 0, VK_INDEX_TYPE_UINT32);

  auto pipelineLayout = pipeline->getDescriptorSetLayout();
  auto cameraLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                   [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                     return info.first == std::string("camera");
                                   });
  if (cameraLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->getPipelineLayout(), 0, 1,
                            &_descriptorSetCameraCubemap[face]->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  auto textureLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                    [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                      return info.first == std::string("texture");
                                    });
  if (textureLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(),
        1, 1, &_material->getDescriptorSetTextures(currentFrame)->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame],
                   static_cast<uint32_t>(_mesh3D->getIndexData().size()), 1, 0, 0, 0);
}

void IBL::drawSpecular() {
  if (_commandBufferTransfer->getActive() == false) throw std::runtime_error("Command buffer isn't in record state");
  _loggerGPU->setCommandBufferName("Draw specular buffer", _commandBufferTransfer);

  auto currentFrame = _state->getFrameInFlight();
  vkCmdBindPipeline(_commandBufferTransfer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipelineSpecular->getPipeline());

  /////////////////////////////////////////////////////////////////////////////////////////
  // render graphic
  /////////////////////////////////////////////////////////////////////////////////////////
  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < _state->getSettings()->getSpecularMipMap(); j++) {
      auto renderPassInfo = _renderPass->getRenderPassInfo(_frameBufferSpecular[i][j]);
      VkClearValue clearColor;
      clearColor.color = _state->getSettings()->getClearColor();
      renderPassInfo.clearValueCount = 1;
      renderPassInfo.pClearValues = &clearColor;

      // TODO: only one depth texture?
      vkCmdBeginRenderPass(_commandBufferTransfer->getCommandBuffer()[currentFrame], &renderPassInfo,
                           VK_SUBPASS_CONTENTS_INLINE);

      _loggerGPU->begin("Render specular");
      // up is inverted for X and Z because of some specific cubemap Y coordinate stuff
      switch (i) {
        case 0:
          // POSITIVE_X / right
          _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(1.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f));
          break;
        case 1:
          // NEGATIVE_X /left
          _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(-1.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f));
          break;
        case 2:
          // POSITIVE_Y / top
          _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
          break;
        case 3:
          // NEGATIVE_Y / bottom
          _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f), glm::vec3(0.f, 0.f, -1.f));
          break;
        case 4:
          // POSITIVE_Z / near
          _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f), glm::vec3(0.f, -1.f, 0.f));
          break;
        case 5:
          // NEGATIVE_Z / far
          _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, -1.f, 0.f));
          break;
      }

      auto [width, height] = _state->getSettings()->getSpecularIBLResolution();
      width *= std::pow(0.5, j);
      height *= std::pow(0.5, j);
      VkViewport viewport{};
      viewport.x = 0.f;
      viewport.y = 0.f;
      viewport.width = width;
      viewport.height = height;
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;
      vkCmdSetViewport(_commandBufferTransfer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

      VkRect2D scissor{};
      scissor.offset = {0, 0};
      scissor.extent = VkExtent2D(width, height);
      vkCmdSetScissor(_commandBufferTransfer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

      if (_pipelineSpecular->getPushConstants().find("fragment") != _pipelineSpecular->getPushConstants().end()) {
        RoughnessConstants pushConstants;
        float roughness = (float)j / (float)(_state->getSettings()->getSpecularMipMap() - 1);
        pushConstants.roughness = roughness;
        vkCmdPushConstants(_commandBufferTransfer->getCommandBuffer()[currentFrame],
                           _pipelineSpecular->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(RoughnessConstants), &pushConstants);
      }

      _draw(i, _camera, _commandBufferTransfer, _pipelineSpecular);

      _loggerGPU->end();

      vkCmdEndRenderPass(_commandBufferTransfer->getCommandBuffer()[currentFrame]);
    }
  }
}

void IBL::drawDiffuse() {
  if (_commandBufferTransfer->getActive() == false) throw std::runtime_error("Command buffer isn't in record state");
  // render cubemap to diffuse
  _loggerGPU->setCommandBufferName("Draw diffuse buffer", _commandBufferTransfer);
  auto currentFrame = _state->getFrameInFlight();
  vkCmdBindPipeline(_commandBufferTransfer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipelineDiffuse->getPipeline());
  /////////////////////////////////////////////////////////////////////////////////////////
  // render graphic
  /////////////////////////////////////////////////////////////////////////////////////////
  for (int i = 0; i < 6; i++) {
    auto renderPassInfo = _renderPass->getRenderPassInfo(_frameBufferDiffuse[i]);
    VkClearValue clearColor;
    clearColor.color = _state->getSettings()->getClearColor();
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(_commandBufferTransfer->getCommandBuffer()[currentFrame], &renderPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    _loggerGPU->begin("Render diffuse");
    // up is inverted for X and Z because of some specific cubemap Y coordinate stuff
    switch (i) {
      case 0:
        // POSITIVE_X / right
        _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(1.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f));
        break;
      case 1:
        // NEGATIVE_X /left
        _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(-1.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f));
        break;
      case 2:
        // POSITIVE_Y / top
        _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
        break;
      case 3:
        // NEGATIVE_Y / bottom
        _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f), glm::vec3(0.f, 0.f, -1.f));
        break;
      case 4:
        // POSITIVE_Z / near
        _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f), glm::vec3(0.f, -1.f, 0.f));
        break;
      case 5:
        // NEGATIVE_Z / far
        _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, -1.f, 0.f));
        break;
    }
    auto [width, height] = _state->getSettings()->getDiffuseIBLResolution();
    VkViewport viewport{};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(_commandBufferTransfer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = VkExtent2D(width, height);
    vkCmdSetScissor(_commandBufferTransfer->getCommandBuffer()[currentFrame], 0, 1, &scissor);
    _draw(i, _camera, _commandBufferTransfer, _pipelineDiffuse);

    _loggerGPU->end();

    vkCmdEndRenderPass(_commandBufferTransfer->getCommandBuffer()[currentFrame]);
  }
}

void IBL::drawSpecularBRDF() {
  if (_commandBufferTransfer->getActive() == false) throw std::runtime_error("Command buffer isn't in record state");
  _loggerGPU->setCommandBufferName("Draw specular brdf", _commandBufferTransfer);

  auto resolution = _textureSpecularBRDF->getImageView()->getImage()->getResolution();
  int currentFrame = _state->getFrameInFlight();
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = std::get<1>(resolution);
  viewport.width = std::get<0>(resolution);
  viewport.height = -std::get<1>(resolution);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(_commandBufferTransfer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(std::get<0>(resolution), std::get<1>(resolution));
  vkCmdSetScissor(_commandBufferTransfer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  auto renderPassInfo = _renderPass->getRenderPassInfo(_frameBufferBRDF);
  VkClearValue clearColor;
  clearColor.color = _state->getSettings()->getClearColor();
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearColor;

  vkCmdBeginRenderPass(_commandBufferTransfer->getCommandBuffer()[currentFrame], &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  _loggerGPU->begin("Render specular brdf");
  BufferMVP cameraMVP{};
  auto model = glm::scale(_model, glm::vec3(2.f, 2.f, 1.f));
  cameraMVP.model = model;
  cameraMVP.view = _cameraSpecularBRDF->getView();
  cameraMVP.projection = _cameraSpecularBRDF->getProjection();

  void* data;
  vkMapMemory(_state->getDevice()->getLogicalDevice(), _cameraBuffer->getBuffer()[currentFrame]->getMemory(), 0,
              sizeof(cameraMVP), 0, &data);
  memcpy(data, &cameraMVP, sizeof(cameraMVP));
  vkUnmapMemory(_state->getDevice()->getLogicalDevice(), _cameraBuffer->getBuffer()[currentFrame]->getMemory());

  VkBuffer vertexBuffers[] = {_mesh2D->getVertexBuffer()->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(_commandBufferTransfer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(_commandBufferTransfer->getCommandBuffer()[currentFrame],
                       _mesh2D->getIndexBuffer()->getBuffer()->getData(), 0, VK_INDEX_TYPE_UINT32);

  auto pipelineLayout = _pipelineSpecularBRDF->getDescriptorSetLayout();
  vkCmdBindPipeline(_commandBufferTransfer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipelineSpecularBRDF->getPipeline());

  // camera
  auto cameraLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                   [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                     return info.first == std::string("camera");
                                   });
  if (cameraLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(_commandBufferTransfer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipelineSpecularBRDF->getPipelineLayout(), 0, 1,
                            &_descriptorSetCamera->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  vkCmdDrawIndexed(_commandBufferTransfer->getCommandBuffer()[currentFrame],
                   static_cast<uint32_t>(_mesh2D->getIndexData().size()), 1, 0, 0, 0);
  _loggerGPU->end();

  vkCmdEndRenderPass(_commandBufferTransfer->getCommandBuffer()[currentFrame]);
}