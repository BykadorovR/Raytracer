#include "Cube.h"

Cube::Cube(std::vector<VkFormat> renderFormat,
           VkCullModeFlags cullMode,
           VkPolygonMode polygonMode,
           std::shared_ptr<LightManager> lightManager,
           std::shared_ptr<CommandBuffer> commandBufferTransfer,
           std::shared_ptr<State> state) {
  _state = state;
  _mesh = std::make_shared<Mesh3D>(state);
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
  _mesh->setVertices(vertices, commandBufferTransfer);
  _mesh->setIndexes(indices, commandBufferTransfer);
  _defaultMaterialColor = std::make_shared<MaterialColor>(commandBufferTransfer, state);
  _material = _defaultMaterialColor;

  // initialize camera UBO and descriptor sets for draw
  // initialize UBO
  _uniformBuffer.resize(6);
  for (int i = 0; i < 6; i++) {
    _uniformBuffer[i] = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(),
                                                        sizeof(BufferMVP), state->getDevice());
  }
  auto setLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
  setLayout->createUniformBuffer();

  _descriptorSetCamera.resize(6);
  for (int i = 0; i < 6; i++) {
    _descriptorSetCamera[i] = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(), setLayout,
                                                              state->getDescriptorPool(), state->getDevice());
    _descriptorSetCamera[i]->createUniformBuffer(_uniformBuffer[i]);
  }
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

  // initialize draw
  {
    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add("../shaders/cube_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/cube_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipeline = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipeline->createGraphic3D(renderFormat, cullMode, polygonMode,
                               {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                               {std::pair{std::string("camera"), setLayout},
                                std::pair{std::string("texture"), _material->getDescriptorSetLayoutTextures()}},
                               {}, _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }
  // initialize depth directional
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("../shaders/depthCube_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    _pipelineDirectional = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineDirectional->createGraphic3DShadow(
        VK_CULL_MODE_NONE, {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT)}, {{"camera", setLayout}}, {},
        _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }

  // initialize depth point
  {
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["fragment"] = DepthConstants::getPushConstant(0);

    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("../shaders/depthCube_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/depthCube_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelinePoint = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelinePoint->createGraphic3DShadow(VK_CULL_MODE_NONE,
                                          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                          {{"camera", setLayout}}, defaultPushConstants, _mesh->getBindingDescription(),
                                          _mesh->getAttributeDescriptions());
  }
  {
    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add("../shaders/skyboxEquirectangular_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/skyboxEquirectangular_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelineEquirectangular = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineEquirectangular->createGraphic3D(
        renderFormat, cullMode, polygonMode,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        {std::pair{std::string("camera"), setLayout},
         std::pair{std::string("texture"), _material->getDescriptorSetLayoutTextures()}},
        {}, _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }
  {
    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add("../shaders/skyboxDiffuse_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("../shaders/skyboxDiffuse_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelineDiffuse = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineDiffuse->createGraphic3D(renderFormat, cullMode, polygonMode,
                                      {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                       shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                      {std::pair{std::string("camera"), setLayout},
                                       std::pair{std::string("texture"), _material->getDescriptorSetLayoutTextures()}},
                                      {}, _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }
}

void Cube::setMaterial(std::shared_ptr<MaterialColor> material) {
  _material = material;
  _materialType = MaterialType::COLOR;
}

void Cube::setModel(glm::mat4 model) { _model = model; }

void Cube::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

std::shared_ptr<Mesh3D> Cube::getMesh() { return _mesh; }

void Cube::_draw(int currentFrame,
                 std::shared_ptr<Pipeline> pipeline,
                 std::shared_ptr<CommandBuffer> commandBuffer,
                 int face) {
  BufferMVP cameraUBO{};
  cameraUBO.model = _model;
  cameraUBO.view = _camera->getView();
  cameraUBO.projection = _camera->getProjection();

  void* data;
  vkMapMemory(_state->getDevice()->getLogicalDevice(), _uniformBuffer[face]->getBuffer()[currentFrame]->getMemory(), 0,
              sizeof(cameraUBO), 0, &data);
  memcpy(data, &cameraUBO, sizeof(cameraUBO));
  vkUnmapMemory(_state->getDevice()->getLogicalDevice(), _uniformBuffer[face]->getBuffer()[currentFrame]->getMemory());

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
                            &_descriptorSetCamera[face]->getDescriptorSets()[currentFrame], 0, nullptr);
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

  vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_mesh->getIndexData().size()),
                   1, 0, 0, 0);
}

void Cube::draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer) {
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline->getPipeline());
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = std::get<1>(_state->getSettings()->getResolution());
  viewport.width = std::get<0>(_state->getSettings()->getResolution());
  viewport.height = -std::get<1>(_state->getSettings()->getResolution());
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(std::get<0>(_state->getSettings()->getResolution()),
                              std::get<1>(_state->getSettings()->getResolution()));
  vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);
  _draw(currentFrame, _pipeline, commandBuffer, 0);
}

void Cube::drawEquirectangular(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer, int face) {
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipelineEquirectangular->getPipeline());
  auto [width, height] = _state->getSettings()->getDepthResolution();
  VkViewport viewport{};
  viewport.x = 0.f;
  viewport.y = 0.f;
  viewport.width = width;
  viewport.height = height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(width, height);
  vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);
  _draw(currentFrame, _pipelineEquirectangular, commandBuffer, face);
}

void Cube::drawDiffuse(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer, int face) {
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipelineDiffuse->getPipeline());
  auto [width, height] = _state->getSettings()->getDiffuseIBLResolution();
  VkViewport viewport{};
  viewport.x = 0.f;
  viewport.y = 0.f;
  viewport.width = width;
  viewport.height = height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(width, height);
  vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);
  _draw(currentFrame, _pipelineDiffuse, commandBuffer, face);
}

void Cube::drawShadow(int currentFrame,
                      std::shared_ptr<CommandBuffer> commandBuffer,
                      LightType lightType,
                      int lightIndex,
                      int face) {
  auto pipeline = _pipelineDirectional;
  if (lightType == LightType::POINT) pipeline = _pipelinePoint;

  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->getPipeline());
  std::tuple<int, int> resolution;
  if (lightType == LightType::DIRECTIONAL) {
    resolution = _lightManager->getDirectionalLights()[lightIndex]
                     ->getDepthTexture()[currentFrame]
                     ->getImageView()
                     ->getImage()
                     ->getResolution();
  }
  if (lightType == LightType::POINT) {
    resolution = _lightManager->getPointLights()[lightIndex]
                     ->getDepthCubemap()[currentFrame]
                     ->getTexture()
                     ->getImageView()
                     ->getImage()
                     ->getResolution();
  }

  // Cube Maps have been specified to follow the RenderMan specification (for whatever reason),
  // and RenderMan assumes the images' origin being in the upper left so we don't need to swap anything
  // if we swap, we need to change shader as well, so swap there. But we can't do it there because we sample from
  // cubemap and we can't just (1 - y)
  VkViewport viewport{};
  if (lightType == LightType::DIRECTIONAL) {
    viewport.x = 0.0f;
    viewport.y = std::get<1>(resolution);
    viewport.width = std::get<0>(resolution);
    viewport.height = -std::get<1>(resolution);
  } else if (lightType == LightType::POINT) {
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = std::get<0>(resolution);
    viewport.height = std::get<1>(resolution);
  }

  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(std::get<0>(resolution), std::get<1>(resolution));
  vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  if (pipeline->getPushConstants().find("fragment") != pipeline->getPushConstants().end()) {
    if (lightType == LightType::POINT) {
      DepthConstants pushConstants;
      pushConstants.lightPosition = _lightManager->getPointLights()[lightIndex]->getPosition();
      // light camera
      pushConstants.far = _lightManager->getPointLights()[lightIndex]->getFar();
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DepthConstants), &pushConstants);
    }
  }

  glm::mat4 view(1.f);
  glm::mat4 projection(1.f);
  int lightIndexTotal = lightIndex;
  if (lightType == LightType::DIRECTIONAL) {
    view = _lightManager->getDirectionalLights()[lightIndex]->getViewMatrix();
    projection = _lightManager->getDirectionalLights()[lightIndex]->getProjectionMatrix();
  }
  if (lightType == LightType::POINT) {
    lightIndexTotal += _state->getSettings()->getMaxDirectionalLights();
    view = _lightManager->getPointLights()[lightIndex]->getViewMatrix(face);
    projection = _lightManager->getPointLights()[lightIndex]->getProjectionMatrix();
  }

  BufferMVP cameraMVP{};
  cameraMVP.model = _model;
  cameraMVP.view = view;
  cameraMVP.projection = projection;

  void* data;
  vkMapMemory(_state->getDevice()->getLogicalDevice(),
              _cameraUBODepth[lightIndexTotal][face]->getBuffer()[currentFrame]->getMemory(), 0, sizeof(cameraMVP), 0,
              &data);
  memcpy(data, &cameraMVP, sizeof(cameraMVP));
  vkUnmapMemory(_state->getDevice()->getLogicalDevice(),
                _cameraUBODepth[lightIndexTotal][face]->getBuffer()[currentFrame]->getMemory());

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
        0, 1, &_descriptorSetCameraDepth[lightIndexTotal][face]->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_mesh->getIndexData().size()),
                   1, 0, 0, 0);
}