#include "IBL.h"

IBL::IBL(std::vector<VkFormat> renderFormat,
         VkCullModeFlags cullMode,
         std::shared_ptr<LightManager> lightManager,
         std::shared_ptr<CommandBuffer> commandBufferTransfer,
         std::shared_ptr<ResourceManager> resourceManager,
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
  _mesh->setColor(std::vector<glm::vec3>(vertices.size(), glm::vec3(1.f, 1.f, 1.f)), commandBufferTransfer);
  _defaultMaterialColor = std::make_shared<MaterialColor>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
  _defaultMaterialColor->setBaseColor({resourceManager->getCubemapOne()->getTexture()});
  _material = _defaultMaterialColor;

  // initialize camera UBO and descriptor sets for draw
  // initialize UBO
  _uniformBuffer.resize(6);
  for (int i = 0; i < 6; i++) {
    _uniformBuffer[i] = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(),
                                                        sizeof(BufferMVP), state);
  }
  auto setLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
  setLayout->createUniformBuffer();

  _descriptorSetCamera.resize(6);
  for (int i = 0; i < 6; i++) {
    _descriptorSetCamera[i] = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(), setLayout,
                                                              state->getDescriptorPool(), state->getDevice());
    _descriptorSetCamera[i]->createUniformBuffer(_uniformBuffer[i]);
  }

  {
    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add("shaders/skyboxEquirectangular_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/skyboxEquirectangular_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelineEquirectangular = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineEquirectangular->createGraphic3D(
        renderFormat, cullMode, VK_POLYGON_MODE_FILL,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        {std::pair{std::string("camera"), setLayout},
         std::pair{std::string("texture"), _material->getDescriptorSetLayoutTextures()}},
        {}, _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }
  {
    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add("shaders/skyboxDiffuse_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/skyboxDiffuse_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelineDiffuse = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineDiffuse->createGraphic3D(renderFormat, cullMode, VK_POLYGON_MODE_FILL,
                                      {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                       shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                      {std::pair{std::string("camera"), setLayout},
                                       std::pair{std::string("texture"), _material->getDescriptorSetLayoutTextures()}},
                                      {}, _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }
  {
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["fragment"] = RoughnessConstants::getPushConstant(0);

    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add("shaders/skyboxSpecular_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/skyboxSpecular_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelineSpecular = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineSpecular->createGraphic3D(renderFormat, cullMode, VK_POLYGON_MODE_FILL,
                                       {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                        shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                       {std::pair{std::string("camera"), setLayout},
                                        std::pair{std::string("texture"), _material->getDescriptorSetLayoutTextures()}},
                                       defaultPushConstants, _mesh->getBindingDescription(),
                                       _mesh->getAttributeDescriptions());
  }
}

void IBL::setMaterial(std::shared_ptr<MaterialColor> material) {
  _material = material;
  _materialType = MaterialType::COLOR;
}

void IBL::setModel(glm::mat4 model) { _model = model; }

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

void IBL::drawEquirectangular(int face, std::shared_ptr<Camera> camera, std::shared_ptr<CommandBuffer> commandBuffer) {
  auto currentFrame = _state->getFrameInFlight();
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
  _draw(face, camera, commandBuffer, _pipelineEquirectangular);
}

void IBL::drawSpecular(int face,
                       int mipMap,
                       std::shared_ptr<Camera> camera,
                       std::shared_ptr<CommandBuffer> commandBuffer) {
  auto currentFrame = _state->getFrameInFlight();
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipelineSpecular->getPipeline());
  auto [width, height] = _state->getSettings()->getSpecularIBLResolution();
  width *= std::pow(0.5, mipMap);
  height *= std::pow(0.5, mipMap);
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

  if (_pipelineSpecular->getPushConstants().find("fragment") != _pipelineSpecular->getPushConstants().end()) {
    RoughnessConstants pushConstants;
    float roughness = (float)mipMap / (float)(_state->getSettings()->getSpecularMipMap() - 1);
    pushConstants.roughness = roughness;
    vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], _pipelineSpecular->getPipelineLayout(),
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RoughnessConstants), &pushConstants);
  }

  _draw(face, camera, commandBuffer, _pipelineSpecular);
}

void IBL::drawDiffuse(int face, std::shared_ptr<Camera> camera, std::shared_ptr<CommandBuffer> commandBuffer) {
  auto currentFrame = _state->getFrameInFlight();
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
  _draw(face, camera, commandBuffer, _pipelineDiffuse);
}