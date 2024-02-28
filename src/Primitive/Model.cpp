#include "Model.h"
#include <unordered_map>
#undef far

void Model3D::enableDepth(bool enable) { _enableDepth = enable; }
bool Model3D::isDepthEnabled() { return _enableDepth; }

Model3D::Model3D(std::vector<VkFormat> renderFormat,
                 const std::vector<std::shared_ptr<NodeGLTF>>& nodes,
                 const std::vector<std::shared_ptr<Mesh3D>>& meshes,
                 std::shared_ptr<LightManager> lightManager,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<ResourceManager> resourceManager,
                 std::shared_ptr<State> state) {
  _commandBufferTransfer = commandBufferTransfer;
  _state = state;
  _loggerCPU = std::make_shared<LoggerCPU>();
  _nodes = nodes;
  _meshes = meshes;
  _lightManager = lightManager;
  auto settings = _state->getSettings();

  // default material if model doesn't have material at all, we still have to send data to shader
  _defaultMaterialPhong = std::make_shared<MaterialPhong>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
  _defaultMaterialPhong->setBaseColor({resourceManager->getTextureOne()});
  _defaultMaterialPhong->setNormal({resourceManager->getTextureZero()});
  _defaultMaterialPhong->setSpecular({resourceManager->getTextureZero()});
  _defaultMaterialPBR = std::make_shared<MaterialPBR>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
  _defaultMaterialColor = std::make_shared<MaterialColor>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
  _mesh = std::make_shared<Mesh3D>(state);
  _defaultAnimation = std::make_shared<Animation>(std::vector<std::shared_ptr<NodeGLTF>>{},
                                                  std::vector<std::shared_ptr<SkinGLTF>>{},
                                                  std::vector<std::shared_ptr<AnimationGLTF>>{}, state);
  _animation = _defaultAnimation;
  auto layoutCamera = std::make_shared<DescriptorSetLayout>(state->getDevice());
  layoutCamera->createUniformBuffer();
  auto layoutCameraGeometry = std::make_shared<DescriptorSetLayout>(state->getDevice());
  layoutCameraGeometry->createUniformBuffer(VK_SHADER_STAGE_GEOMETRY_BIT);
  // layout for Color
  _descriptorSetLayout[MaterialType::COLOR].push_back({"camera", layoutCamera});
  _descriptorSetLayout[MaterialType::COLOR].push_back({"joint", _defaultAnimation->getDescriptorSetLayoutJoints()});
  _descriptorSetLayout[MaterialType::COLOR].push_back(
      {"texture", _defaultMaterialColor->getDescriptorSetLayoutTextures()});

  // layout for Phong
  _descriptorSetLayout[MaterialType::PHONG].push_back({"camera", layoutCamera});
  _descriptorSetLayout[MaterialType::PHONG].push_back({"joint", _defaultAnimation->getDescriptorSetLayoutJoints()});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"texture", _defaultMaterialPhong->getDescriptorSetLayoutTextures()});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"lightVP", _lightManager->getDSLViewProjection(VK_SHADER_STAGE_VERTEX_BIT)});
  _descriptorSetLayout[MaterialType::PHONG].push_back({"lightPhong", _lightManager->getDSLLightPhong()});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"alphaMask", _defaultMaterialPhong->getDescriptorSetLayoutAlphaCutoff()});
  _descriptorSetLayout[MaterialType::PHONG].push_back({"shadowTexture", _lightManager->getDSLShadowTexture()});
  _descriptorSetLayout[MaterialType::PHONG].push_back(
      {"materialCoefficients", _defaultMaterialPhong->getDescriptorSetLayoutCoefficients()});

  // layout for PBR
  _descriptorSetLayout[MaterialType::PBR].push_back({"camera", layoutCamera});
  _descriptorSetLayout[MaterialType::PBR].push_back({"joint", _defaultAnimation->getDescriptorSetLayoutJoints()});
  _descriptorSetLayout[MaterialType::PBR].push_back({"texture", _defaultMaterialPBR->getDescriptorSetLayoutTextures()});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {"lightVP", _lightManager->getDSLViewProjection(VK_SHADER_STAGE_VERTEX_BIT)});
  _descriptorSetLayout[MaterialType::PBR].push_back({"lightPBR", _lightManager->getDSLLightPBR()});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {"alphaMask", _defaultMaterialPBR->getDescriptorSetLayoutAlphaCutoff()});
  _descriptorSetLayout[MaterialType::PBR].push_back({"shadowTexture", _lightManager->getDSLShadowTexture()});
  _descriptorSetLayout[MaterialType::PBR].push_back(
      {"materialCoefficients", _defaultMaterialPBR->getDescriptorSetLayoutCoefficients()});

  // layout for Normal
  _descriptorSetLayoutNormal.push_back({"camera", layoutCamera});
  _descriptorSetLayoutNormal.push_back({"cameraGeometry", layoutCameraGeometry});
  // initialize Color
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("shaders/model/modelColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/model/modelColor_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[MaterialType::COLOR] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());

    _pipeline[MaterialType::COLOR]->createGraphic3D(renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
                                                    {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                     shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                    _descriptorSetLayout[MaterialType::COLOR], {},
                                                    _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());

    _pipelineCullOff[MaterialType::COLOR] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    _pipelineCullOff[MaterialType::COLOR]->createGraphic3D(renderFormat, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
                                                           {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                            shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                           _descriptorSetLayout[MaterialType::COLOR], {},
                                                           _mesh->getBindingDescription(),
                                                           _mesh->getAttributeDescriptions());

    _pipelineWireframe[MaterialType::COLOR] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    _pipelineWireframe[MaterialType::COLOR]->createGraphic3D(renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
                                                             {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                              shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                             _descriptorSetLayout[MaterialType::COLOR], {},
                                                             _mesh->getBindingDescription(),
                                                             _mesh->getAttributeDescriptions());
  }

  // initialize Phong
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("shaders/model/modelPhong_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/model/modelPhong_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[MaterialType::PHONG] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["fragment"] = LightPush::getPushConstant();

    _pipeline[MaterialType::PHONG]->createGraphic3D(renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
                                                    {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                     shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                    _descriptorSetLayout[MaterialType::PHONG], defaultPushConstants,
                                                    _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());

    _pipelineCullOff[MaterialType::PHONG] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    _pipelineCullOff[MaterialType::PHONG]->createGraphic3D(renderFormat, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
                                                           {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                            shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                           _descriptorSetLayout[MaterialType::PHONG],
                                                           defaultPushConstants, _mesh->getBindingDescription(),
                                                           _mesh->getAttributeDescriptions());

    _pipelineWireframe[MaterialType::PHONG] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    _pipelineWireframe[MaterialType::PHONG]->createGraphic3D(renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_LINE,
                                                             {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                              shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                             _descriptorSetLayout[MaterialType::PHONG],
                                                             defaultPushConstants, _mesh->getBindingDescription(),
                                                             _mesh->getAttributeDescriptions());
  }
  // initialize PBR
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("shaders/model/modelPBR_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/model/modelPBR_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[MaterialType::PBR] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["fragment"] = LightPush::getPushConstant();

    _pipeline[MaterialType::PBR]->createGraphic3D(renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
                                                  {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                   shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                  _descriptorSetLayout[MaterialType::PBR], defaultPushConstants,
                                                  _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());

    _pipelineCullOff[MaterialType::PBR] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    _pipelineCullOff[MaterialType::PBR]->createGraphic3D(renderFormat, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
                                                         {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                          shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                         _descriptorSetLayout[MaterialType::PBR], defaultPushConstants,
                                                         _mesh->getBindingDescription(),
                                                         _mesh->getAttributeDescriptions());

    _pipelineWireframe[MaterialType::PBR] = std::make_shared<Pipeline>(state->getSettings(), state->getDevice());
    _pipelineWireframe[MaterialType::PBR]->createGraphic3D(renderFormat, VK_CULL_MODE_NONE, VK_POLYGON_MODE_LINE,
                                                           {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                            shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                           _descriptorSetLayout[MaterialType::PBR],
                                                           defaultPushConstants, _mesh->getBindingDescription(),
                                                           _mesh->getAttributeDescriptions());
  }

  // initialize Normal (per vertex)
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("shaders/shape/cubeNormal_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/shape/cubeNormal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    shader->add("shaders/shape/cubeNormal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

    _pipelineNormalMesh = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineNormalMesh->createGraphic3D(renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
                                         {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                          shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
                                          shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
                                         _descriptorSetLayoutNormal, {}, _mesh->getBindingDescription(),
                                         _mesh->getAttributeDescriptions());

    // it's kind of pointless, but if material is set, double sided property can be handled
    _pipelineNormalMeshCullOff = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineNormalMeshCullOff->createGraphic3D(renderFormat, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
                                                {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                 shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
                                                 shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
                                                _descriptorSetLayoutNormal, {}, _mesh->getBindingDescription(),
                                                _mesh->getAttributeDescriptions());
  }

  // initialize Tangent (per vertex)
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("shaders/shape/cubeTangent_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/shape/cubeNormal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    shader->add("shaders/shape/cubeNormal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

    _pipelineTangentMesh = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineTangentMesh->createGraphic3D(renderFormat, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL,
                                          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
                                           shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
                                          _descriptorSetLayoutNormal, {}, _mesh->getBindingDescription(),
                                          _mesh->getAttributeDescriptions());

    // it's kind of pointless, but if material is set, double sided property can be handled
    _pipelineTangentMeshCullOff = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineTangentMeshCullOff->createGraphic3D(renderFormat, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
                                                 {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                  shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
                                                  shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
                                                 _descriptorSetLayoutNormal, {}, _mesh->getBindingDescription(),
                                                 _mesh->getAttributeDescriptions());
  }

  // initialize depth directional
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("shaders/model/modelDepth_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    _pipelineDirectional = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineDirectional->createGraphic3DShadow(
        VK_CULL_MODE_NONE, {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT)},
        {{"camera", layoutCamera}, {"joint", _defaultAnimation->getDescriptorSetLayoutJoints()}}, {},
        _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }

  // initialize depth point
  {
    auto shader = std::make_shared<Shader>(_state->getDevice());
    shader->add("shaders/model/modelDepth_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/model/modelDepth_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelinePoint = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["fragment"] = DepthConstants::getPushConstant(0);
    _pipelinePoint->createGraphic3DShadow(
        VK_CULL_MODE_NONE,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        {{"camera", layoutCamera}, {"joint", _defaultAnimation->getDescriptorSetLayoutJoints()}}, defaultPushConstants,
        _mesh->getBindingDescription(), _mesh->getAttributeDescriptions());
  }

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

void Model3D::_drawNode(std::shared_ptr<CommandBuffer> commandBuffer,
                        std::shared_ptr<Pipeline> pipeline,
                        std::shared_ptr<Pipeline> pipelineCullOff,
                        std::shared_ptr<DescriptorSet> cameraDS,
                        std::shared_ptr<UniformBuffer> cameraUBO,
                        glm::mat4 view,
                        glm::mat4 projection,
                        std::shared_ptr<NodeGLTF> node) {
  int currentFrame = _state->getFrameInFlight();
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
          pipeline->getPipelineLayout(), 1, 1,
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
                                  pipeline->getPipelineLayout(), 2, 1,
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
    _drawNode(commandBuffer, pipeline, pipelineCullOff, cameraDS, cameraUBO, view, projection, child);
  }
}

void Model3D::enableShadow(bool enable) { _enableShadow = enable; }

void Model3D::enableLighting(bool enable) { _enableLighting = enable; }

void Model3D::draw(std::tuple<int, int> resolution,
                   std::shared_ptr<Camera> camera,
                   std::shared_ptr<CommandBuffer> commandBuffer) {
  auto pipeline = _pipeline[_materialType];
  auto pipelineCullOff = _pipelineCullOff[_materialType];
  if (_drawType == DrawType::WIREFRAME) pipeline = _pipelineWireframe[_materialType];
  if (_drawType == DrawType::NORMAL) {
    pipeline = _pipelineNormalMesh;
    pipelineCullOff = _pipelineNormalMeshCullOff;
  }
  if (_drawType == DrawType::TANGENT) {
    pipeline = _pipelineTangentMesh;
    pipelineCullOff = _pipelineTangentMeshCullOff;
  }

  int currentFrame = _state->getFrameInFlight();
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

  if (pipeline->getPushConstants().find("fragment") != pipeline->getPushConstants().end()) {
    LightPush pushConstants;
    pushConstants.enableShadow = _enableShadow;
    pushConstants.enableLighting = _enableLighting;
    pushConstants.cameraPosition = camera->getEye();
    vkCmdPushConstants(commandBuffer->getCommandBuffer()[_state->getFrameInFlight()], pipeline->getPipelineLayout(),
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LightPush), &pushConstants);
  }

  auto pipelineLayout = pipeline->getDescriptorSetLayout();
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->getPipeline());
  auto lightVPLayout = std::find_if(
      pipelineLayout.begin(), pipelineLayout.end(),
      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "lightVP"; });
  if (lightVPLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(),
        3, 1, &_lightManager->getDSViewProjection(VK_SHADER_STAGE_VERTEX_BIT)->getDescriptorSets()[currentFrame], 0,
        nullptr);
  }

  auto lightLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                  [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                    return info.first == std::string("lightPhong");
                                  });
  if (lightLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->getPipelineLayout(), 4, 1,
                            &_lightManager->getDSLightPhong()->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  lightLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                             [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                               return info.first == std::string("lightPBR");
                             });
  if (lightLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->getPipelineLayout(), 4, 1,
                            &_lightManager->getDSLightPBR()->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  auto shadowTextureLayout = std::find_if(
      pipelineLayout.begin(), pipelineLayout.end(),
      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) { return info.first == "shadowTexture"; });
  if (shadowTextureLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(),
        6, 1, &_lightManager->getDSShadowTexture()[currentFrame]->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  // Render all nodes at top-level
  for (auto& node : _nodes) {
    _drawNode(commandBuffer, pipeline, pipelineCullOff, _descriptorSetCameraFull, _cameraUBOFull, camera->getView(),
              camera->getProjection(), node);
  }
}

void Model3D::drawShadow(LightType lightType, int lightIndex, int face, std::shared_ptr<CommandBuffer> commandBuffer) {
  if (_enableDepth == false) return;

  int currentFrame = _state->getFrameInFlight();
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

  if (lightType == LightType::POINT) {
    if (pipeline->getPushConstants().find("fragment") != pipeline->getPushConstants().end()) {
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
  // Render all nodes at top-level
  for (auto& node : _nodes) {
    _drawNode(commandBuffer, pipeline, pipeline, _descriptorSetCameraDepth[lightIndexTotal][face],
              _cameraUBODepth[lightIndexTotal][face], view, projection, node);
  }
}