#include "Model.h"
#include <unordered_map>

void Model3D::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }
void Model3D::setModel(glm::mat4 model) { _model = model; }
void Model3D::enableDepth(bool enable) { _enableDepth = enable; }
bool Model3D::isDepthEnabled() { return _enableDepth; }

Model3D::Model3D(const std::vector<std::shared_ptr<NodeGLTF>>& nodes,
                 const std::vector<std::shared_ptr<Mesh3D>>& meshes,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<ResourceManager> resourceManager,
                 std::shared_ptr<State> state) {
  _commandBufferTransfer = commandBufferTransfer;
  _state = state;
  _loggerCPU = std::make_shared<LoggerCPU>();
  _nodes = nodes;
  _meshes = meshes;

  // default material if model doesn't have material at all, we still have to send data to shader
  _defaultMaterialPhong = std::make_shared<MaterialPhong>(commandBufferTransfer, state);
  _defaultMaterialPhong->setBaseColor(resourceManager->getTextureOne());
  _defaultMaterialPhong->setNormal(resourceManager->getTextureZero());
  _defaultMaterialPhong->setSpecular(resourceManager->getTextureZero());

  // initialize camera UBO and descriptor sets for shadow
  // initialize UBO
  int lightNumber = _state->getSettings()->getMaxDirectionalLights() + _state->getSettings()->getMaxPointLights();
  for (int i = 0; i < _state->getSettings()->getMaxDirectionalLights(); i++) {
    _cameraUBODepth.push_back(
        {std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(BufferMVP), _state)});
  }

  for (int i = 0; i < _state->getSettings()->getMaxPointLights(); i++) {
    std::vector<std::shared_ptr<UniformBuffer>> facesBuffer(6);
    for (int j = 0; j < 6; j++) {
      facesBuffer[j] = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(BufferMVP),
                                                       _state);
    }
    _cameraUBODepth.push_back(facesBuffer);
  }
  // initialize descriptor sets
  auto layoutCamera = std::make_shared<DescriptorSetLayout>(state->getDevice());
  layoutCamera->createUniformBuffer();
  auto layoutCameraGeometry = std::make_shared<DescriptorSetLayout>(state->getDevice());
  layoutCameraGeometry->createUniformBuffer(VK_SHADER_STAGE_GEOMETRY_BIT);
  {
    for (int i = 0; i < _state->getSettings()->getMaxDirectionalLights(); i++) {
      auto cameraSet = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(), layoutCamera,
                                                       _state->getDescriptorPool(), _state->getDevice());
      cameraSet->createUniformBuffer(_cameraUBODepth[i][0]);

      _descriptorSetCameraDepth.push_back({cameraSet});
    }

    for (int i = 0; i < _state->getSettings()->getMaxPointLights(); i++) {
      std::vector<std::shared_ptr<DescriptorSet>> facesSet(6);
      for (int j = 0; j < 6; j++) {
        facesSet[j] = std::make_shared<DescriptorSet>(_state->getSettings()->getMaxFramesInFlight(), layoutCamera,
                                                      _state->getDescriptorPool(), _state->getDevice());
        facesSet[j]->createUniformBuffer(_cameraUBODepth[i + _state->getSettings()->getMaxDirectionalLights()][j]);
      }
      _descriptorSetCameraDepth.push_back(facesSet);
    }
  }

  // initialize camera UBO and descriptor sets for draw pass
  // initialize UBO
  _cameraUBOFull = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(BufferMVP),
                                                   _state);
  // initialize descriptor sets
  {
    _descriptorSetCameraFull = std::make_shared<DescriptorSet>(
        _state->getSettings()->getMaxFramesInFlight(), layoutCamera, _state->getDescriptorPool(), _state->getDevice());
    _descriptorSetCameraFull->createUniformBuffer(_cameraUBOFull);

    _descriptorSetCameraGeometry = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(),
                                                                   layoutCameraGeometry, state->getDescriptorPool(),
                                                                   state->getDevice());
    _descriptorSetCameraGeometry->createUniformBuffer(_cameraUBOFull);
  }
}

void Model3D::setMaterial(std::vector<std::shared_ptr<MaterialColor>> materials) {
  _materials.clear();
  for (auto& material : materials) {
    _materials.push_back(material);
  }
  _materialType = MaterialType::COLOR;
}

void Model3D::setMaterial(std::vector<std::shared_ptr<MaterialPhong>> materials) {
  _materials.clear();
  for (auto& material : materials) {
    _materials.push_back(material);
  }
  _materialType = MaterialType::PHONG;
}

void Model3D::setMaterial(std::vector<std::shared_ptr<MaterialPBR>> materials) {
  _materials.clear();
  for (auto& material : materials) {
    _materials.push_back(material);
  }
  _materialType = MaterialType::PBR;
}

void Model3D::setDrawType(DrawType drawType) { _drawType = drawType; }

MaterialType Model3D::getMaterialType() { return _materialType; }

DrawType Model3D::getDrawType() { return _drawType; }

void Model3D::setAnimation(std::shared_ptr<Animation> animation) { _animation = animation; }

void Model3D::_drawNode(int currentFrame,
                        std::shared_ptr<CommandBuffer> commandBuffer,
                        std::shared_ptr<Pipeline> pipeline,
                        std::shared_ptr<Pipeline> pipelineCullOff,
                        std::shared_ptr<DescriptorSet> cameraDS,
                        std::shared_ptr<UniformBuffer> cameraUBO,
                        glm::mat4 view,
                        glm::mat4 projection,
                        std::shared_ptr<NodeGLTF> node) {
  if (node->mesh >= 0 && _meshes[node->mesh]->getPrimitives().size() > 0) {
    VkBuffer vertexBuffers[] = {_meshes[node->mesh]->getVertexBuffer()->getBuffer()->getData()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame],
                         _meshes[node->mesh]->getIndexBuffer()->getBuffer()->getData(), 0, VK_INDEX_TYPE_UINT32);

    glm::mat4 nodeMatrix = node->getLocalMatrix();
    std::shared_ptr<NodeGLTF> currentParent = node->parent;
    while (currentParent) {
      nodeMatrix = currentParent->getLocalMatrix() * nodeMatrix;
      currentParent = currentParent->parent;
    }
    // pass this matrix to uniforms
    BufferMVP cameraMVP{};
    cameraMVP.model = _model * nodeMatrix;
    cameraMVP.view = view;
    cameraMVP.projection = projection;

    void* data;
    vkMapMemory(_state->getDevice()->getLogicalDevice(), cameraUBO->getBuffer()[currentFrame]->getMemory(), 0,
                sizeof(cameraMVP), 0, &data);
    memcpy(data, &cameraMVP, sizeof(cameraMVP));
    vkUnmapMemory(_state->getDevice()->getLogicalDevice(), cameraUBO->getBuffer()[currentFrame]->getMemory());

    auto pipelineLayout = pipeline->getDescriptorSetLayout();
    // camera
    auto cameraLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                     [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                       return info.first == std::string("camera");
                                     });
    if (cameraLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 0, 1, &cameraDS->getDescriptorSets()[currentFrame], 0,
                              nullptr);
    }

    // camera geometry
    auto cameraLayoutGeometry = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                             [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                               return info.first == std::string("cameraGeometry");
                                             });
    if (cameraLayoutGeometry != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 1, 1,
                              &_descriptorSetCameraGeometry->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    auto jointLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                    [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                      return info.first == std::string("joint");
                                    });

    if (jointLayout != pipelineLayout.end()) {
      // if node->skin == -1 then use 0 index that contains identity matrix because of animation default behavior
      vkCmdBindDescriptorSets(
          commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
          pipeline->getPipelineLayout(), 3, 1,
          &_animation->getDescriptorSetJoints()[std::max(0, node->skin)]->getDescriptorSets()[currentFrame], 0,
          nullptr);
    }

    auto alphaMaskLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                          return info.first == std::string("alphaMask");
                                        });
    auto textureLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                        return info.first == std::string("texture");
                                      });
    auto materialCoefficientsLayout = std::find_if(
        pipelineLayout.begin(), pipelineLayout.end(),
        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
          return info.first == std::string("materialCoefficients");
        });

    for (MeshPrimitive primitive : _meshes[node->mesh]->getPrimitives()) {
      if (primitive.indexCount > 0) {
        std::shared_ptr<Material> material = _defaultMaterialPhong;
        // Get the texture index for this primitive
        if (_materials.size() > 0) {
          material = _materials.front();
          if (primitive.materialIndex >= 0 && primitive.materialIndex < _materials.size())
            material = _materials[primitive.materialIndex];
        }

        // assign alpha cutoff from material
        if (alphaMaskLayout != pipelineLayout.end()) {
          vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipeline->getPipelineLayout(), 5, 1,
                                  &material->getDescriptorSetAlphaCutoff()->getDescriptorSets()[currentFrame], 0,
                                  nullptr);
        }
        // assign material textures
        if (textureLayout != pipelineLayout.end()) {
          vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipeline->getPipelineLayout(), 1, 1,
                                  &material->getDescriptorSetTextures(currentFrame)->getDescriptorSets()[currentFrame],
                                  0, nullptr);
        }
        // assign coefficients
        if (materialCoefficientsLayout != pipelineLayout.end()) {
          vkCmdBindDescriptorSets(
              commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
              pipeline->getPipelineLayout(), 7, 1,
              &material->getDescriptorSetCoefficients(currentFrame)->getDescriptorSets()[currentFrame], 0, nullptr);
        }

        auto currentPipeline = pipeline;
        // by default double side is false
        if (material->getDoubleSided()) currentPipeline = pipelineCullOff;
        vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                          currentPipeline->getPipeline());
        vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], primitive.indexCount, 1, primitive.firstIndex,
                         0, 0);
      }
    }
  }

  for (auto& child : node->children) {
    _drawNode(currentFrame, commandBuffer, pipeline, pipelineCullOff, cameraDS, cameraUBO, view, projection, child);
  }
}

void Model3D::enableShadow(bool enable) { _enableShadow = enable; }

void Model3D::enableLighting(bool enable) { _enableLighting = enable; }

void Model3D::draw(int currentFrame,
                   std::shared_ptr<CommandBuffer> commandBuffer,
                   std::shared_ptr<Pipeline> pipeline,
                   std::shared_ptr<Pipeline> pipelineCullOff) {
  if (pipeline->getPushConstants().find("fragment") != pipeline->getPushConstants().end()) {
    LightPush pushConstants;
    pushConstants.enableShadow = _enableShadow;
    pushConstants.enableLighting = _enableLighting;
    pushConstants.cameraPosition = _camera->getEye();
    vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LightPush), &pushConstants);
  }

  // Render all nodes at top-level
  for (auto& node : _nodes) {
    _drawNode(currentFrame, commandBuffer, pipeline, pipelineCullOff, _descriptorSetCameraFull, _cameraUBOFull,
              _camera->getView(), _camera->getProjection(), node);
  }
}

void Model3D::drawShadow(int currentFrame,
                         std::shared_ptr<CommandBuffer> commandBuffer,
                         std::shared_ptr<Pipeline> pipeline,
                         std::shared_ptr<Pipeline> pipelineCullOff,
                         int lightIndex,
                         glm::mat4 view,
                         glm::mat4 projection,
                         int face) {
  // Render all nodes at top-level
  for (auto& node : _nodes) {
    _drawNode(currentFrame, commandBuffer, pipeline, pipelineCullOff, _descriptorSetCameraDepth[lightIndex][face],
              _cameraUBODepth[lightIndex][face], view, projection, node);
  }
}