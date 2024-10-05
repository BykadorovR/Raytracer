#include "Primitive/Skybox.h"

Skybox::Skybox(std::shared_ptr<CommandBuffer> commandBufferTransfer,
               std::shared_ptr<GameState> gameState,
               std::shared_ptr<State> state) {
  _state = state;
  _gameState = gameState;
  _mesh = std::make_shared<MeshStatic3D>(state);
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
  _defaultMaterialColor = std::make_shared<MaterialColor>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
  _defaultMaterialColor->setBaseColor({gameState->getResourceManager()->getTextureOne()});
  _material = _defaultMaterialColor;
  _renderPass = std::make_shared<RenderPass>(_state->getSettings(), _state->getDevice());
  _renderPass->initializeGraphic();

  _uniformBuffer = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(BufferMVP),
                                                   state);

  // setup color
  {
    _descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_state->getDevice());
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
    _descriptorSetLayout->createCustom(layoutColor);

    _descriptorSet = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(), _descriptorSetLayout,
                                                     state->getDescriptorPool(), state->getDevice());

    _updateColorDescriptor({_defaultMaterialColor});

    // initialize Color
    {
      auto shader = std::make_shared<Shader>(state);
      shader->add("shaders/skybox/skybox_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/skybox/skybox_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

      _pipeline = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
      _pipeline->createSkybox(
          VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          {std::pair{std::string("color"), _descriptorSetLayout}}, {}, _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)}}), _renderPass);
    }
  }
}

void Skybox::_updateColorDescriptor(std::shared_ptr<MaterialColor> material) {
  for (int i = 0; i < _state->getSettings()->getMaxFramesInFlight(); i++) {
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor;
    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor;
    std::vector<VkDescriptorBufferInfo> bufferInfoCamera(1);
    // write to binding = 0 for vertex shader
    bufferInfoCamera[0].buffer = _uniformBuffer->getBuffer()[i]->getData();
    bufferInfoCamera[0].offset = 0;
    bufferInfoCamera[0].range = sizeof(BufferMVP);
    bufferInfoColor[0] = bufferInfoCamera;

    // write for binding = 1 for textures
    std::vector<VkDescriptorImageInfo> bufferInfoTexture(1);
    bufferInfoTexture[0].imageLayout = material->getBaseColor()[0]->getImageView()->getImage()->getImageLayout();
    bufferInfoTexture[0].imageView = material->getBaseColor()[0]->getImageView()->getImageView();
    bufferInfoTexture[0].sampler = material->getBaseColor()[0]->getSampler()->getSampler();
    textureInfoColor[1] = bufferInfoTexture;
    _descriptorSet->createCustom(i, bufferInfoColor, textureInfoColor);
  }
}

void Skybox::setMaterial(std::shared_ptr<MaterialColor> material) {
  _material = material;
  _materialType = MaterialType::COLOR;
  _updateColorDescriptor(material);
}

void Skybox::setModel(glm::mat4 model) { _model = model; }

std::shared_ptr<MeshStatic3D> Skybox::getMesh() { return _mesh; }

void Skybox::draw(std::shared_ptr<CommandBuffer> commandBuffer) {
  auto currentFrame = _state->getFrameInFlight();
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

  BufferMVP cameraUBO{};
  cameraUBO.model = _model;
  cameraUBO.view = glm::mat4(glm::mat3(_gameState->getCameraManager()->getCurrentCamera()->getView()));
  cameraUBO.projection = _gameState->getCameraManager()->getCurrentCamera()->getProjection();

  _uniformBuffer->getBuffer()[currentFrame]->setData(&cameraUBO);

  VkBuffer vertexBuffers[] = {_mesh->getVertexBuffer()->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame], _mesh->getIndexBuffer()->getBuffer()->getData(),
                       0, VK_INDEX_TYPE_UINT32);

  auto pipelineLayout = _pipeline->getDescriptorSetLayout();
  auto cameraLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                   [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                     return info.first == std::string("color");
                                   });
  if (cameraLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline->getPipelineLayout(), 0, 1, &_descriptorSet->getDescriptorSets()[currentFrame], 0,
                            nullptr);
  }

  vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_mesh->getIndexData().size()),
                   1, 0, 0, 0);
}