#include "Graphic/IBL.h"

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
      {Vertex2D{{0.5f, 0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}, {1.f, 0.f, 0.f, 1.f}},
       Vertex2D{{0.5f, -0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}, {1.f, 0.f, 0.f, 1.f}},
       Vertex2D{{-0.5f, -0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}, {1.f, 0.f, 0.f, 1.f}},
       Vertex2D{{-0.5f, 0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}, {1.f, 0.f, 0.f, 1.f}}},
      commandBufferTransfer);
  _mesh2D->setIndexes({0, 3, 2, 2, 1, 0}, commandBufferTransfer);

  // TODO: should be texture zero??
  _material = std::make_shared<MaterialColor>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
  std::dynamic_pointer_cast<MaterialColor>(_material)->setBaseColor({resourceManager->getCubemapOne()->getTexture()});
  _renderPass = std::make_shared<RenderPass>(_state->getSettings(), _state->getDevice());
  _renderPass->initializeIBL();
  _cameraBuffer = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(BufferMVP),
                                                  state);
  // setup BRDF
  {
    auto cameraLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
    cameraLayout->createUniformBuffer();
    _descriptorSetBRDF = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(), cameraLayout,
                                                         state->getDescriptorPool(), state->getDevice());
    _descriptorSetBRDF->createUniformBuffer(_cameraBuffer);

    {
      auto shader = std::make_shared<Shader>(state);
      shader->add("shaders/IBL/specularBRDF_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/IBL/specularBRDF_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      _pipelineSpecularBRDF = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      _pipelineSpecularBRDF->createGraphic2D(
          VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, true,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          {{"brdf", cameraLayout}}, {}, _mesh2D->getBindingDescription(),
          _mesh2D->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex2D, pos)},
                                                   {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex2D, texCoord)}}),
          _renderPass);
    }
  }

  // initialize camera UBO and descriptor sets for draw
  // initialize UBO
  _cameraBufferCubemap.resize(6);
  for (int i = 0; i < 6; i++) {
    _cameraBufferCubemap[i] = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(),
                                                              sizeof(BufferMVP), state);
  }

  // setup diffuse and specular
  {
    _descriptorSetLayoutColor = std::make_shared<DescriptorSetLayout>(_state->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutColor(2);
    layoutColor[0].binding = 0;
    layoutColor[0].descriptorCount = 1;
    layoutColor[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutColor[0].pImmutableSamplers = nullptr;
    layoutColor[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layoutColor[1].binding = 1;
    layoutColor[1].descriptorCount = 1;
    layoutColor[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutColor[1].pImmutableSamplers = nullptr;
    layoutColor[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    _descriptorSetLayoutColor->createCustom(layoutColor);

    _descriptorSetColor.resize(6);
    for (int f = 0; f < 6; f++) {
      _descriptorSetColor[f] = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                               _descriptorSetLayoutColor, state->getDescriptorPool(),
                                                               state->getDevice());
    }

    _updateColorDescriptor(_material);

    {
      auto shader = std::make_shared<Shader>(state);
      shader->add("shaders/IBL/skyboxDiffuse_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/IBL/skyboxDiffuse_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      _pipelineDiffuse = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      _pipelineDiffuse->createGraphic3D(
          VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          {std::pair{std::string("color"), _descriptorSetLayoutColor}}, {}, _mesh3D->getBindingDescription(),
          _mesh3D->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)}}),
          _renderPass);
    }
    {
      std::map<std::string, VkPushConstantRange> defaultPushConstants;
      defaultPushConstants["fragment"] = RoughnessConstants::getPushConstant(0);

      auto shader = std::make_shared<Shader>(state);
      shader->add("shaders/IBL/skyboxSpecular_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/IBL/skyboxSpecular_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      _pipelineSpecular = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      _pipelineSpecular->createGraphic3D(
          VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          {std::pair{std::string("color"), _descriptorSetLayoutColor}}, defaultPushConstants,
          _mesh3D->getBindingDescription(),
          _mesh3D->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)}}),
          _renderPass);
    }
  }

  _logger = std::make_shared<Logger>(state);

  _cubemapDiffuse = std::make_shared<Cubemap>(
      _state->getSettings()->getDiffuseIBLResolution(), _state->getSettings()->getGraphicColorFormat(), 1,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FILTER_LINEAR, commandBufferTransfer, state);
  _cubemapSpecular = std::make_shared<Cubemap>(
      _state->getSettings()->getSpecularIBLResolution(), _state->getSettings()->getGraphicColorFormat(),
      _state->getSettings()->getSpecularMipMap(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FILTER_LINEAR, commandBufferTransfer, state);
  auto brdfImage = std::make_shared<Image>(_state->getSettings()->getDepthResolution(), 1, 1,
                                           _state->getSettings()->getGraphicColorFormat(), VK_IMAGE_TILING_OPTIMAL,
                                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state);
  brdfImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, commandBufferTransfer);
  auto brdfImageView = std::make_shared<ImageView>(brdfImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                   VK_IMAGE_ASPECT_COLOR_BIT, state);
  _textureSpecularBRDF = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, VK_FILTER_LINEAR,
                                                   brdfImageView, state);

  _cameraSpecularBRDF = std::make_shared<CameraOrtho>();
  _cameraSpecularBRDF->setProjectionParameters({-1, 1, 1, -1}, 0, 1);
  _cameraSpecularBRDF->setViewParameters(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));

  _camera = std::make_shared<CameraFly>(_state);
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

void IBL::_updateColorDescriptor(std::shared_ptr<MaterialColor> material) {
  for (int f = 0; f < 6; f++) {
    _descriptorSetColor[f] = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(),
                                                             _descriptorSetLayoutColor, _state->getDescriptorPool(),
                                                             _state->getDevice());
    for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
      std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
      std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
      std::vector<VkDescriptorBufferInfo> bufferInfoCamera(1);
      // write to binding = 0 for vertex shader
      bufferInfoCamera[0].buffer = _cameraBufferCubemap[f]->getBuffer()[i]->getData();
      bufferInfoCamera[0].offset = 0;
      bufferInfoCamera[0].range = sizeof(BufferMVP);
      bufferInfoColor[0] = bufferInfoCamera;

      // write for binding = 1 for textures
      std::vector<VkDescriptorImageInfo> bufferInfoTexture(1);
      bufferInfoTexture[0].imageLayout = _material->getBaseColor()[0]->getImageView()->getImage()->getImageLayout();
      bufferInfoTexture[0].imageView = _material->getBaseColor()[0]->getImageView()->getImageView();
      bufferInfoTexture[0].sampler = _material->getBaseColor()[0]->getSampler()->getSampler();
      textureInfoColor[1] = bufferInfoTexture;
      _descriptorSetColor[f]->createCustom(i, bufferInfoColor, textureInfoColor);
    }
  }
}

void IBL::setMaterial(std::shared_ptr<MaterialColor> material) {
  _material = material;
  _updateColorDescriptor(material);
}

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

  _cameraBufferCubemap[face]->getBuffer()[currentFrame]->setData(&cameraUBO);

  VkBuffer vertexBuffers[] = {_mesh3D->getVertexBuffer()->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame],
                       _mesh3D->getIndexBuffer()->getBuffer()->getData(), 0, VK_INDEX_TYPE_UINT32);

  auto pipelineLayout = pipeline->getDescriptorSetLayout();
  auto cameraLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                   [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                     return info.first == std::string("color");
                                   });
  if (cameraLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->getPipelineLayout(), 0, 1,
                            &_descriptorSetColor[face]->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame],
                   static_cast<uint32_t>(_mesh3D->getIndexData().size()), 1, 0, 0, 0);
}

void IBL::drawSpecular() {
  if (_commandBufferTransfer->getActive() == false) throw std::runtime_error("Command buffer isn't in record state");

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

      _logger->begin("Render specular", _commandBufferTransfer);
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

      _logger->end(_commandBufferTransfer);

      vkCmdEndRenderPass(_commandBufferTransfer->getCommandBuffer()[currentFrame]);
    }
  }
}

void IBL::drawDiffuse() {
  if (_commandBufferTransfer->getActive() == false) throw std::runtime_error("Command buffer isn't in record state");
  // render cubemap to diffuse
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

    _logger->begin("Render diffuse", _commandBufferTransfer);
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

    _logger->end(_commandBufferTransfer);

    vkCmdEndRenderPass(_commandBufferTransfer->getCommandBuffer()[currentFrame]);
  }
}

void IBL::drawSpecularBRDF() {
  if (_commandBufferTransfer->getActive() == false) throw std::runtime_error("Command buffer isn't in record state");

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

  _logger->begin("Render specular brdf", _commandBufferTransfer);
  BufferMVP cameraMVP{};
  auto model = glm::scale(_model, glm::vec3(2.f, 2.f, 1.f));
  cameraMVP.model = model;
  cameraMVP.view = _cameraSpecularBRDF->getView();
  cameraMVP.projection = _cameraSpecularBRDF->getProjection();

  _cameraBuffer->getBuffer()[currentFrame]->setData(&cameraMVP);

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
                                     return info.first == std::string("brdf");
                                   });
  if (cameraLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(_commandBufferTransfer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipelineSpecularBRDF->getPipelineLayout(), 0, 1,
                            &_descriptorSetBRDF->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  vkCmdDrawIndexed(_commandBufferTransfer->getCommandBuffer()[currentFrame],
                   static_cast<uint32_t>(_mesh2D->getIndexData().size()), 1, 0, 0, 0);
  _logger->end(_commandBufferTransfer);

  vkCmdEndRenderPass(_commandBufferTransfer->getCommandBuffer()[currentFrame]);
}