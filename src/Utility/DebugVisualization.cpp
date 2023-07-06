#include "DebugVisualization.h"

DebugVisualization::DebugVisualization(std::shared_ptr<Camera> camera,
                                       std::shared_ptr<GUI> gui,
                                       std::shared_ptr<CommandBuffer> commandBufferTransfer,
                                       std::shared_ptr<State> state) {
  _camera = camera;
  _gui = gui;
  _state = state;
  _commandBufferTransfer = commandBufferTransfer;

  auto shader = std::make_shared<Shader>(state->getDevice());
  shader->add("../shaders/quad2D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader->add("../shaders/quad2D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  _uniformBuffer = std::make_shared<UniformBuffer>(state->getSettings()->getMaxFramesInFlight(), sizeof(glm::mat4),
                                                   state->getDevice());
  _vertexBuffer = std::make_shared<VertexBuffer2D>(_vertices, commandBufferTransfer, state->getDevice());
  _indexBuffer = std::make_shared<IndexBuffer>(_indices, commandBufferTransfer, state->getDevice());

  auto cameraLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
  cameraLayout->createCamera();

  _cameraSet = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(), cameraLayout,
                                               state->getDescriptorPool(), state->getDevice());
  _cameraSet->createCamera(_uniformBuffer);

  _textureSetLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
  _textureSetLayout->createGraphic();

  _pipeline = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
  _pipeline->createHUD(
      {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
       shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
      {{"camera", cameraLayout}, {"texture", _textureSetLayout}},
      std::map<std::string, VkPushConstantRange>{{std::string("fragment"), DepthPush::getPushConstant()}},
      Vertex2D::getBindingDescription(), Vertex2D::getAttributeDescriptions());

  for (auto elem : _state->getSettings()->getAttenuations()) {
    _attenuationKeys.push_back(std::to_string(std::get<0>(elem)));
  }
}

void DebugVisualization::setTexture(std::shared_ptr<Texture> texture) {
  _texture = texture;
  _textureSet = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(), _textureSetLayout,
                                                _state->getDescriptorPool(), _state->getDevice());
  _textureSet->createGraphic(texture);
}

void DebugVisualization::setLights(std::shared_ptr<Model3DManager> modelManager,
                                   std::shared_ptr<LightManager> lightManager) {
  _modelManager = modelManager;
  _lightManager = lightManager;
  for (auto light : lightManager->getPointLights()) {
    auto model = _modelManager->createModelGLTF("../data/Box/Box.gltf");
    model->setDebug(true);
    model->enableShadow(false);
    model->enableLighting(false);
    _modelManager->registerModelGLTF(model);
    _pointLightModels.push_back(model);

    auto sphere = std::make_shared<Sphere>(_commandBufferTransfer, _state);
    sphere->setCamera(_camera);
    _spheres.push_back(sphere);
  }

  for (auto light : lightManager->getDirectionalLights()) {
    auto model = _modelManager->createModelGLTF("../data/Box/Box.gltf");
    model->setDebug(true);
    model->enableShadow(false);
    model->enableLighting(false);
    _modelManager->registerModelGLTF(model);
    _directionalLightModels.push_back(model);
  }
}

void DebugVisualization::draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer) {
  std::map<std::string, bool*> toggleDepth;
  toggleDepth["Depth"] = &_showDepth;
  _gui->drawCheckbox("Debug", {20, 100}, toggleDepth);

  if (_lightManager) {
    std::map<std::string, bool*> toggle;
    toggle["Lights"] = &_showLights;
    _gui->drawCheckbox("Debug", {20, 100}, toggle);
    if (_showLights) {
      std::map<std::string, bool*> toggle;
      toggle["Spheres"] = &_enableSpheres;
      _gui->drawCheckbox("Debug", {20, 100}, toggle);
      if (_enableSpheres) {
        std::map<std::string, int*> toggleSpheres;
        toggleSpheres["##Spheres"] = &_lightSpheresIndex;
        _gui->drawListBox("Debug Spheres", {20, 220}, _attenuationKeys, toggleSpheres);
      }
      for (int i = 0; i < _lightManager->getPointLights().size(); i++) {
        if (_registerLights) _modelManager->registerModelGLTF(_pointLightModels[i]);
        {
          auto model = glm::translate(glm::mat4(1.f), _lightManager->getPointLights()[i]->getPosition());
          model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f));
          _pointLightModels[i]->setModel(model);
        }
        {
          auto model = glm::translate(glm::mat4(1.f), _lightManager->getPointLights()[i]->getPosition());
          if (_enableSpheres) {
            if (_lightSpheresIndex < 0) _lightSpheresIndex = _lightManager->getPointLights()[i]->getAttenuationIndex();
            _lightManager->getPointLights()[i]->setAttenuationIndex(_lightSpheresIndex);
            int distance = _lightManager->getPointLights()[i]->getDistance();
            model = glm::scale(model, glm::vec3(distance, distance, distance));
            _spheres[i]->setModel(model);
            _spheres[i]->draw(currentFrame, commandBuffer);
          }
        }
      }
      for (int i = 0; i < _lightManager->getDirectionalLights().size(); i++) {
        if (_registerLights) _modelManager->registerModelGLTF(_directionalLightModels[i]);
        auto model = glm::translate(glm::mat4(1.f), _lightManager->getDirectionalLights()[i]->getPosition());
        model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f));
        _directionalLightModels[i]->setModel(model);
      }
      _registerLights = false;
    } else {
      if (_registerLights == false) {
        _registerLights = true;
        for (int i = 0; i < _lightManager->getPointLights().size(); i++) {
          _modelManager->unregisterModelGLTF(_pointLightModels[i]);
        }
        for (int i = 0; i < _lightManager->getDirectionalLights().size(); i++) {
          _modelManager->unregisterModelGLTF(_directionalLightModels[i]);
        }
      }
    }
  }

  if (_texture != nullptr && _showDepth) {
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

    DepthPush pushConstants;
    pushConstants.near = _camera->getNear();
    pushConstants.far = _camera->getFar();
    vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], _pipeline->getPipelineLayout(),
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DepthPush), &pushConstants);

    glm::mat4 model = glm::mat4(1.f);
    model = glm::translate(model, glm::vec3(0.75f, -0.75f, 0.f));
    model = glm::scale(model, glm::vec3(0.5f, 0.5f, 1.f));

    void* data;
    vkMapMemory(_state->getDevice()->getLogicalDevice(), _uniformBuffer->getBuffer()[currentFrame]->getMemory(), 0,
                sizeof(glm::mat4), 0, &data);
    memcpy(data, &model, sizeof(glm::mat4));
    vkUnmapMemory(_state->getDevice()->getLogicalDevice(), _uniformBuffer->getBuffer()[currentFrame]->getMemory());

    VkBuffer vertexBuffers[] = {_vertexBuffer->getBuffer()->getData()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame], _indexBuffer->getBuffer()->getData(), 0,
                         VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline->getPipelineLayout(), 0, 1, &_cameraSet->getDescriptorSets()[currentFrame], 0,
                            nullptr);

    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline->getPipelineLayout(), 1, 1, &_textureSet->getDescriptorSets()[currentFrame], 0,
                            nullptr);

    vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_indices.size()), 1, 0, 0,
                     0);
  }
}

void DebugVisualization::cursorNotify(GLFWwindow* window, float xPos, float yPos) {}

void DebugVisualization::mouseNotify(GLFWwindow* window, int button, int action, int mods) {}

void DebugVisualization::keyNotify(GLFWwindow* window, int key, int action, int mods) {
  if ((action == GLFW_RELEASE && key == GLFW_KEY_C)) {
    if (_cursorEnabled) {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      _cursorEnabled = false;
    } else {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      _cursorEnabled = true;
    }
  }
}