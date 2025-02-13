#include "Primitive/Model.h"
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <unordered_map>
#undef far

struct FragmentPointLightPushDepth {
  alignas(16) glm::vec3 lightPosition;
  alignas(16) int far;
};

struct FragmentPush {
  int enableShadow;
  int enableLighting;
  alignas(16) glm::vec3 cameraPosition;
};

Model3DPhysics::Model3DPhysics(glm::vec3 translate, glm::vec3 size, std::shared_ptr<PhysicsManager> physicsManager) {
  _physicsManager = physicsManager;
  JPH::CharacterSettings settings;
  _size = size;
  settings.mShape = new JPH::BoxShape(JPH::Vec3(size.x / 2.f, size.y / 2.f, size.z / 2.f));
  settings.mLayer = Layers::MOVING;
  settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -size.y / 2.f);
  _character = new JPH::Character(&settings, JPH::Vec3(translate.x, translate.y, translate.z), JPH::Quat::sIdentity(),
                                  0, &_physicsManager->getPhysicsSystem());
  _character->AddToPhysicsSystem(JPH::EActivation::Activate);
}

Model3DPhysics::Model3DPhysics(glm::vec3 translate,
                               float height,
                               float radius,
                               std::shared_ptr<PhysicsManager> physicsManager) {
  _physicsManager = physicsManager;
  JPH::CharacterSettings settings;
  _size = {radius * 2.f, height + radius * 2.f, radius * 2.f};
  settings.mShape = new JPH::CapsuleShape(height / 2.f, radius);
  settings.mLayer = Layers::MOVING;
  settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -radius);
  _character = new JPH::Character(&settings, JPH::Vec3(translate.x, translate.y, translate.z), JPH::Quat::sIdentity(),
                                  0, &_physicsManager->getPhysicsSystem());
  _character->AddToPhysicsSystem(JPH::EActivation::Activate);
}

void Model3DPhysics::setTranslate(glm::vec3 translate) {
  _physicsManager->getBodyInterface().SetPosition(
      _character->GetBodyID(), JPH::RVec3(translate.x, translate.y, translate.z), JPH::EActivation::Activate);
}

void Model3DPhysics::setRotate(glm::quat rotate) {
  _physicsManager->getBodyInterface().SetRotation(
      _character->GetBodyID(), JPH::Quat(rotate.x, rotate.y, rotate.z, rotate.w), JPH::EActivation::Activate);
}

glm::quat Model3DPhysics::getRotate() {
  auto rotation = _physicsManager->getBodyInterface().GetRotation(_character->GetBodyID());
  return glm::quat{rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ()};
}

glm::vec3 Model3DPhysics::getTranslate() {
  auto position = _physicsManager->getBodyInterface().GetPosition(_character->GetBodyID());
  return glm::vec3(position.GetX(), position.GetY(), position.GetZ());
}

glm::vec3 Model3DPhysics::getSize() { return _size; }

void Model3DPhysics::postUpdate() { _character->PostSimulation(_collisionTolerance); }

void Model3DPhysics::setFriction(float friction) {
  _physicsManager->getBodyInterface().SetFriction(_character->GetBodyID(), friction);
}

GroundState Model3DPhysics::getGroundState() { return (GroundState)_character->GetGroundState(); }

glm::vec3 Model3DPhysics::getGroundNormal() {
  JPH::Vec3 normal = _character->GetGroundNormal();
  return glm::vec3{normal.GetX(), normal.GetY(), normal.GetZ()};
}

void Model3DPhysics::setUp(glm::vec3 up) { _character->SetUp({up.x, up.y, up.z}); }

glm::vec3 Model3DPhysics::getGroundVelocity() {
  auto velocity = _character->GetGroundVelocity();
  return {velocity.GetX(), velocity.GetY(), velocity.GetZ()};
}

void Model3DPhysics::setLinearVelocity(glm::vec3 velocity) {
  _physicsManager->getBodyInterface().SetLinearVelocity(_character->GetBodyID(), {velocity.x, velocity.y, velocity.z});
}

glm::vec3 Model3DPhysics::getUp() {
  auto up = _character->GetUp();
  return {up.GetX(), up.GetY(), up.GetZ()};
}

glm::vec3 Model3DPhysics::getLinearVelocity() {
  auto velocity = _physicsManager->getBodyInterface().GetLinearVelocity(_character->GetBodyID());
  return {velocity.GetX(), velocity.GetY(), velocity.GetZ()};
}

Model3DPhysics::~Model3DPhysics() {
  _character->RemoveFromPhysicsSystem();
  // destroy body isn't needed because it's called automatically in Character destructor, removeFromPhysicsSystem does
  // removeBody inside
}

void Model3D::enableDepth(bool enable) { _enableDepth = enable; }

bool Model3D::isDepthEnabled() { return _enableDepth; }

Model3D::Model3D(const std::vector<std::shared_ptr<NodeGLTF>>& nodes,
                 const std::vector<std::shared_ptr<MeshStatic3D>>& meshes,
                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                 std::shared_ptr<GameState> gameState,
                 std::shared_ptr<EngineState> engineState) {
  setName("Model3D");
  _engineState = engineState;
  _nodes = nodes;
  _meshes = meshes;
  _gameState = gameState;
  auto settings = _engineState->getSettings();
  _changedMaterial.resize(_engineState->getSettings()->getMaxFramesInFlight(), false);
  // default material if model doesn't have material at all, we still have to send data to shader
  _defaultMaterialPhong = std::make_shared<MaterialPhong>(MaterialTarget::SIMPLE, commandBufferTransfer, engineState);
  _defaultMaterialPhong->setBaseColor({_gameState->getResourceManager()->getTextureOne()});
  _defaultMaterialPhong->setNormal({_gameState->getResourceManager()->getTextureZero()});
  _defaultMaterialPhong->setSpecular({_gameState->getResourceManager()->getTextureZero()});
  _defaultMaterialPBR = std::make_shared<MaterialPBR>(MaterialTarget::SIMPLE, commandBufferTransfer, engineState);
  _defaultMaterialColor = std::make_shared<MaterialColor>(MaterialTarget::SIMPLE, commandBufferTransfer, engineState);
  _materials = {_defaultMaterialPhong};

  _mesh = std::make_shared<MeshStatic3D>(engineState);
  _defaultAnimation = std::make_shared<Animation>(std::vector<std::shared_ptr<NodeGLTF>>{},
                                                  std::vector<std::shared_ptr<SkinGLTF>>{},
                                                  std::vector<std::shared_ptr<AnimationGLTF>>{}, engineState);
  _animation = _defaultAnimation;
  _renderPass = _engineState->getRenderPassManager()->getRenderPass(RenderPassScenario::GRAPHIC);
  _renderPassDepth = _engineState->getRenderPassManager()->getRenderPass(RenderPassScenario::SHADOW);

  // initialize UBO
  _cameraUBOFull.resize(_engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++)
    _cameraUBOFull[i] = std::make_shared<Buffer>(
        sizeof(BufferMVP), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);

  // setup joints
  {
    _descriptorSetLayoutJoints = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutJoints{{.binding = 0,
                                                            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                            .descriptorCount = 1,
                                                            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                                            .pImmutableSamplers = nullptr}};
    _descriptorSetLayoutJoints->createCustom(layoutJoints);

    _updateJointsDescriptor();
  }

  // setup Normal
  {
    _descriptorSetLayoutNormalsMesh = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutNormalsMesh{{.binding = 0,
                                                                 .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                                 .descriptorCount = 1,
                                                                 .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                                                 .pImmutableSamplers = nullptr},
                                                                {.binding = 1,
                                                                 .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                                 .descriptorCount = 1,
                                                                 .stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT,
                                                                 .pImmutableSamplers = nullptr}};
    _descriptorSetLayoutNormalsMesh->createCustom(layoutNormalsMesh);

    // TODO: we can just have one buffer and put it twice to descriptor

    _descriptorSetNormalsMesh = std::make_shared<DescriptorSet>(engineState->getSettings()->getMaxFramesInFlight(),
                                                                _descriptorSetLayoutNormalsMesh, engineState);
    for (int i = 0; i < engineState->getSettings()->getMaxFramesInFlight(); i++) {
      std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoNormalsMesh = {
          {0,
           {VkDescriptorBufferInfo{.buffer = _cameraUBOFull[i]->getData(),
                                   .offset = 0,
                                   .range = _cameraUBOFull[i]->getSize()}}},
          {1,
           {VkDescriptorBufferInfo{.buffer = _cameraUBOFull[i]->getData(),
                                   .offset = 0,
                                   .range = _cameraUBOFull[i]->getSize()}}}};
      _descriptorSetNormalsMesh->createCustom(i, bufferInfoNormalsMesh, {});
    }

    // initialize Normal (per vertex)
    auto shader = std::make_shared<Shader>(_engineState);
    shader->add("shaders/shape/cubeNormal_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/shape/cubeNormal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    shader->add("shaders/shape/cubeNormal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

    _pipelineNormalMesh = std::make_shared<PipelineGraphic>(_engineState->getDevice());
    _pipelineNormalMesh->setDepthTest(true);
    _pipelineNormalMesh->setDepthWrite(true);
    _pipelineNormalMesh->setCullMode(VK_CULL_MODE_BACK_BIT);
    _pipelineNormalMesh->createCustom(
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
        std::vector{std::pair{std::string("normal"), _descriptorSetLayoutNormalsMesh}}, {},
        _mesh->getBindingDescription(),
        _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                               {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, normal)},
                                               {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)}}),
        _renderPass);

    // it's kind of pointless, but if material is set, double sided property can be handled
    _pipelineNormalMeshCullOff = std::make_shared<PipelineGraphic>(_engineState->getDevice());
    _pipelineNormalMeshCullOff->setDepthTest(true);
    _pipelineNormalMeshCullOff->setDepthWrite(true);
    _pipelineNormalMeshCullOff->createCustom(
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
        std::vector{std::pair{std::string("normal"), _descriptorSetLayoutNormalsMesh}}, {},
        _mesh->getBindingDescription(),
        _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                               {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, normal)},
                                               {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)}}),
        _renderPass);

    // initialize Tangent (per vertex)
    {
      auto shader = std::make_shared<Shader>(_engineState);
      shader->add("shaders/shape/cubeTangent_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/shape/cubeNormal_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
      shader->add("shaders/shape/cubeNormal_geometry.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

      _pipelineTangentMesh = std::make_shared<PipelineGraphic>(_engineState->getDevice());
      _pipelineTangentMesh->setDepthTest(true);
      _pipelineTangentMesh->setDepthWrite(true);
      _pipelineTangentMesh->setCullMode(VK_CULL_MODE_BACK_BIT);
      _pipelineTangentMesh->createCustom(
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
          std::vector{std::pair{std::string("normal"), _descriptorSetLayoutNormalsMesh}}, {},
          _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, normal)},
                                                 {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)}}),
          _renderPass);

      // it's kind of pointless, but if material is set, double sided property can be handled
      _pipelineTangentMeshCullOff = std::make_shared<PipelineGraphic>(_engineState->getDevice());
      _pipelineTangentMeshCullOff->setDepthTest(true);
      _pipelineTangentMeshCullOff->setDepthWrite(true);
      _pipelineTangentMeshCullOff->createCustom(
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT)},
          std::vector{std::pair{std::string("normal"), _descriptorSetLayoutNormalsMesh}}, {},
          _mesh->getBindingDescription(),
          _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                                 {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, normal)},
                                                 {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)}}),
          _renderPass);
    }
  }

  // setup color
  {
    _descriptorSetLayoutColor = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutColor{{.binding = 0,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                                           .pImmutableSamplers = nullptr},
                                                          {.binding = 1,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                           .pImmutableSamplers = nullptr}};
    _descriptorSetLayoutColor->createCustom(layoutColor);

    // phong is default, will form default descriptor only for it here
    // descriptors are formed in setMaterial

    // initialize Color
    {
      auto shader = std::make_shared<Shader>(_engineState);
      shader->add("shaders/model/modelColor_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/model/modelColor_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

      _pipeline[MaterialType::COLOR] = std::make_shared<PipelineGraphic>(engineState->getDevice());
      std::vector<std::tuple<VkFormat, uint32_t>> attributes = {
          {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
          {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, color)},
          {VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex3D, texCoord)},
          {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex3D, jointIndices)},
          {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex3D, jointWeights)}};
      _pipeline[MaterialType::COLOR]->setDepthTest(true);
      _pipeline[MaterialType::COLOR]->setDepthWrite(true);
      _pipeline[MaterialType::COLOR]->setCullMode(VK_CULL_MODE_BACK_BIT);
      _pipeline[MaterialType::COLOR]->createCustom(
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          {{"color", _descriptorSetLayoutColor}, {"joints", _descriptorSetLayoutJoints}}, {},
          _mesh->getBindingDescription(), _mesh->Mesh::getAttributeDescriptions(attributes), _renderPass);

      _pipelineCullOff[MaterialType::COLOR] = std::make_shared<PipelineGraphic>(engineState->getDevice());
      _pipelineCullOff[MaterialType::COLOR]->setDepthTest(true);
      _pipelineCullOff[MaterialType::COLOR]->setDepthWrite(true);
      _pipelineCullOff[MaterialType::COLOR]->createCustom(
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          {{"color", _descriptorSetLayoutColor}, {"joints", _descriptorSetLayoutJoints}}, {},
          _mesh->getBindingDescription(), _mesh->Mesh::getAttributeDescriptions(attributes), _renderPass);

      _pipelineWireframe[MaterialType::COLOR] = std::make_shared<PipelineGraphic>(engineState->getDevice());
      _pipelineWireframe[MaterialType::COLOR]->setDepthTest(true);
      _pipelineWireframe[MaterialType::COLOR]->setDepthWrite(true);
      _pipelineWireframe[MaterialType::COLOR]->setCullMode(VK_CULL_MODE_BACK_BIT);
      _pipelineWireframe[MaterialType::COLOR]->setPolygonMode(VK_POLYGON_MODE_LINE);
      _pipelineWireframe[MaterialType::COLOR]->createCustom(
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          {{"color", _descriptorSetLayoutColor}, {"joints", _descriptorSetLayoutJoints}}, {},
          _mesh->getBindingDescription(), _mesh->Mesh::getAttributeDescriptions(attributes), _renderPass);
    }
  }

  // setup Phong
  {
    _descriptorSetLayoutPhong = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutPhong{{.binding = 0,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                                           .pImmutableSamplers = nullptr},
                                                          {.binding = 1,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                           .pImmutableSamplers = nullptr},
                                                          {.binding = 2,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                           .pImmutableSamplers = nullptr},
                                                          {.binding = 3,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                           .pImmutableSamplers = nullptr},
                                                          {.binding = 4,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                           .pImmutableSamplers = nullptr},
                                                          {.binding = 5,
                                                           .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                           .descriptorCount = 1,
                                                           .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                           .pImmutableSamplers = nullptr}};
    _descriptorSetLayoutPhong->createCustom(layoutPhong);

    setMaterial({_defaultMaterialPhong});

    auto shader = std::make_shared<Shader>(_engineState);
    shader->add("shaders/model/modelPhong_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/model/modelPhong_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    _pipeline[MaterialType::PHONG] = std::make_shared<PipelineGraphic>(engineState->getDevice());
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["constants"] =
        VkPushConstantRange{.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = sizeof(FragmentPush)};
    _pipeline[MaterialType::PHONG]->setDepthTest(true);
    _pipeline[MaterialType::PHONG]->setDepthWrite(true);
    _pipeline[MaterialType::PHONG]->setCullMode(VK_CULL_MODE_BACK_BIT);
    _pipeline[MaterialType::PHONG]->createCustom({shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                  shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                 {{"phong", _descriptorSetLayoutPhong},
                                                  {"joints", _descriptorSetLayoutJoints},
                                                  {"globalPhong", _gameState->getLightManager()->getDSLGlobalPhong()}},
                                                 defaultPushConstants, _mesh->getBindingDescription(),
                                                 _mesh->getAttributeDescriptions(), _renderPass);

    _pipelineCullOff[MaterialType::PHONG] = std::make_shared<PipelineGraphic>(engineState->getDevice());
    _pipelineCullOff[MaterialType::PHONG]->setDepthTest(true);
    _pipelineCullOff[MaterialType::PHONG]->setDepthWrite(true);
    _pipelineCullOff[MaterialType::PHONG]->createCustom(
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        {{"phong", _descriptorSetLayoutPhong},
         {"joints", _descriptorSetLayoutJoints},
         {"globalPhong", _gameState->getLightManager()->getDSLGlobalPhong()}},
        defaultPushConstants, _mesh->getBindingDescription(), _mesh->getAttributeDescriptions(), _renderPass);

    _pipelineWireframe[MaterialType::PHONG] = std::make_shared<PipelineGraphic>(engineState->getDevice());
    _pipelineWireframe[MaterialType::PHONG]->setDepthTest(true);
    _pipelineWireframe[MaterialType::PHONG]->setDepthWrite(true);
    _pipelineWireframe[MaterialType::PHONG]->setCullMode(VK_CULL_MODE_BACK_BIT);
    _pipelineWireframe[MaterialType::PHONG]->setPolygonMode(VK_POLYGON_MODE_LINE);
    _pipelineWireframe[MaterialType::PHONG]->createCustom(
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        {{"phong", _descriptorSetLayoutPhong},
         {"joints", _descriptorSetLayoutJoints},
         {"globalPhong", _gameState->getLightManager()->getDSLGlobalPhong()}},
        defaultPushConstants, _mesh->getBindingDescription(), _mesh->getAttributeDescriptions(), _renderPass);
  }

  // setup PBR
  {
    _descriptorSetLayoutPBR = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
    std::vector<VkDescriptorSetLayoutBinding> layoutPBR{{.binding = 0,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 1,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 2,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 3,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 4,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 5,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 6,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 7,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 8,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 9,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 10,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr},
                                                        {.binding = 11,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                         .descriptorCount = 1,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                         .pImmutableSamplers = nullptr}};

    _descriptorSetLayoutPBR->createCustom(layoutPBR);

    // phong is default, will form default descriptor only for it here
    // descriptors are formed in setMaterial

    // initialize PBR
    {
      auto shader = std::make_shared<Shader>(_engineState);
      shader->add("shaders/model/modelPBR_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
      shader->add("shaders/model/modelPBR_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

      _pipeline[MaterialType::PBR] = std::make_shared<PipelineGraphic>(engineState->getDevice());
      _pipeline[MaterialType::PBR]->setDepthTest(true);
      _pipeline[MaterialType::PBR]->setDepthWrite(true);
      _pipeline[MaterialType::PBR]->setCullMode(VK_CULL_MODE_BACK_BIT);

      std::map<std::string, VkPushConstantRange> defaultPushConstants;
      defaultPushConstants["constants"] =
          VkPushConstantRange{.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = sizeof(FragmentPush)};

      _pipeline[MaterialType::PBR]->createCustom({shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                                                  shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                                                 {{"pbr", _descriptorSetLayoutPBR},
                                                  {"joints", _descriptorSetLayoutJoints},
                                                  {"globalPBR", _gameState->getLightManager()->getDSLGlobalPBR()}},
                                                 defaultPushConstants, _mesh->getBindingDescription(),
                                                 _mesh->getAttributeDescriptions(), _renderPass);

      _pipelineCullOff[MaterialType::PBR] = std::make_shared<PipelineGraphic>(engineState->getDevice());
      _pipelineCullOff[MaterialType::PBR]->setDepthTest(true);
      _pipelineCullOff[MaterialType::PBR]->setDepthWrite(true);
      _pipelineCullOff[MaterialType::PBR]->createCustom(
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          {{"pbr", _descriptorSetLayoutPBR},
           {"joints", _descriptorSetLayoutJoints},
           {"globalPBR", _gameState->getLightManager()->getDSLGlobalPBR()}},
          defaultPushConstants, _mesh->getBindingDescription(), _mesh->getAttributeDescriptions(), _renderPass);

      _pipelineWireframe[MaterialType::PBR] = std::make_shared<PipelineGraphic>(engineState->getDevice());
      _pipelineWireframe[MaterialType::PBR]->setDepthTest(true);
      _pipelineWireframe[MaterialType::PBR]->setDepthWrite(true);
      _pipelineWireframe[MaterialType::PBR]->setPolygonMode(VK_POLYGON_MODE_LINE);
      _pipelineWireframe[MaterialType::PBR]->createCustom(
          {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
           shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
          {{"pbr", _descriptorSetLayoutPBR},
           {"joints", _descriptorSetLayoutJoints},
           {"globalPBR", _gameState->getLightManager()->getDSLGlobalPBR()}},
          defaultPushConstants, _mesh->getBindingDescription(), _mesh->getAttributeDescriptions(), _renderPass);
    }
  }

  auto cameraLayout = std::make_shared<DescriptorSetLayout>(engineState->getDevice());
  VkDescriptorSetLayoutBinding layoutBinding = {.binding = 0,
                                                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                .descriptorCount = 1,
                                                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                                .pImmutableSamplers = nullptr};
  cameraLayout->createCustom({layoutBinding});

  int lightNumber = _engineState->getSettings()->getMaxDirectionalLights() +
                    _engineState->getSettings()->getMaxPointLights();
  for (int i = 0; i < _engineState->getSettings()->getMaxDirectionalLights(); i++) {
    std::vector<std::shared_ptr<Buffer>> buffer(_engineState->getSettings()->getMaxFramesInFlight());
    for (int j = 0; j < _engineState->getSettings()->getMaxFramesInFlight(); j++)
      buffer[j] = std::make_shared<Buffer>(sizeof(BufferMVP), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                           _engineState);
    _cameraUBODepth.push_back({buffer});
  }

  for (int i = 0; i < _engineState->getSettings()->getMaxPointLights(); i++) {
    std::vector<std::vector<std::shared_ptr<Buffer>>> facesBuffer(6);
    for (int j = 0; j < 6; j++) {
      facesBuffer[j].resize(_engineState->getSettings()->getMaxFramesInFlight());
      for (int k = 0; k < _engineState->getSettings()->getMaxFramesInFlight(); k++)
        facesBuffer[j][k] = std::make_shared<Buffer>(
            sizeof(BufferMVP), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);
    }
    _cameraUBODepth.push_back(facesBuffer);
  }
  // initialize descriptor sets
  {
    for (int i = 0; i < _engineState->getSettings()->getMaxDirectionalLights(); i++) {
      auto cameraSet = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                       cameraLayout, _engineState);
      for (int j = 0; j < _engineState->getSettings()->getMaxFramesInFlight(); j++) {
        std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfo = {
            {0,
             {{.buffer = _cameraUBODepth[i][0][j]->getData(),
               .offset = 0,
               .range = _cameraUBODepth[i][0][j]->getSize()}}}};
        cameraSet->createCustom(j, bufferInfo, {});
      }
      _descriptorSetCameraDepth.push_back({cameraSet});
    }

    for (int i = 0; i < _engineState->getSettings()->getMaxPointLights(); i++) {
      std::vector<std::shared_ptr<DescriptorSet>> facesSet(6);
      for (int j = 0; j < 6; j++) {
        facesSet[j] = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(), cameraLayout,
                                                      _engineState);
        for (int k = 0; k < _engineState->getSettings()->getMaxFramesInFlight(); k++) {
          std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfo = {
              {0,
               {{.buffer = _cameraUBODepth[i + _engineState->getSettings()->getMaxDirectionalLights()][j][k]->getData(),
                 .offset = 0,
                 .range =
                     _cameraUBODepth[i + _engineState->getSettings()->getMaxDirectionalLights()][j][k]->getSize()}}}};
          facesSet[j]->createCustom(k, bufferInfo, {});
        }
      }
      _descriptorSetCameraDepth.push_back(facesSet);
    }
  }

  // initialize depth directional
  {
    auto shader = std::make_shared<Shader>(_engineState);
    shader->add("shaders/model/modelDepth_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/model/modelDepthDirectional_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelineDirectional = std::make_shared<PipelineGraphic>(_engineState->getDevice());
    _pipelineDirectional->setDepthBias(true);
    _pipelineDirectional->setColorBlendOp(VK_BLEND_OP_MIN);
    _pipelineDirectional->createCustom(
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        {{"depth", cameraLayout}, {"joints", _descriptorSetLayoutJoints}}, {}, _mesh->getBindingDescription(),
        _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                               {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex3D, jointIndices)},
                                               {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex3D, jointWeights)}}),
        _renderPassDepth);
  }

  // initialize depth point
  {
    auto shader = std::make_shared<Shader>(_engineState);
    shader->add("shaders/model/modelDepth_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/model/modelDepthPoint_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelinePoint = std::make_shared<PipelineGraphic>(_engineState->getDevice());
    std::map<std::string, VkPushConstantRange> defaultPushConstants;
    defaultPushConstants["constants"] = VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(FragmentPointLightPushDepth),
    };

    _pipelinePoint->setDepthBias(true);
    _pipelinePoint->setColorBlendOp(VK_BLEND_OP_MIN);
    _pipelinePoint->createCustom(
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        {{"depth", cameraLayout}, {"joints", _descriptorSetLayoutJoints}}, defaultPushConstants,
        _mesh->getBindingDescription(),
        _mesh->Mesh::getAttributeDescriptions({{VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, pos)},
                                               {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex3D, jointIndices)},
                                               {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex3D, jointWeights)}}),
        _renderPassDepth);
  }
}

void Model3D::_updateJointsDescriptor() {
  _descriptorSetJoints.resize(_animation->getJointMatricesBuffer().size());
  for (int skin = 0; skin < _animation->getJointMatricesBuffer().size(); skin++) {
    _descriptorSetJoints[skin] = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                                 _descriptorSetLayoutJoints, _engineState);
    for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++) {
      std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfo = {
          {0,
           {{.buffer = _animation->getJointMatricesBuffer()[skin][i]->getData(),
             .offset = 0,
             .range = _animation->getJointMatricesBuffer()[skin][i]->getSize()}}}};
      _descriptorSetJoints[skin]->createCustom(i, bufferInfo, {});
    }
  }
}

void Model3D::_updatePBRDescriptor() {
  int currentFrame = _engineState->getFrameInFlight();
  for (int i = 0; i < _materials.size(); i++) {
    std::shared_ptr<MaterialPBR> material = std::dynamic_pointer_cast<MaterialPBR>(_materials[i]);
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor = {
        {0,
         {{.buffer = _cameraUBOFull[currentFrame]->getData(),
           .offset = 0,
           .range = _cameraUBOFull[currentFrame]->getSize()}}},
        {10,
         {{.buffer = material->getBufferAlphaCutoff()[currentFrame]->getData(),
           .offset = 0,
           .range = material->getBufferAlphaCutoff()[currentFrame]->getSize()}}},
        {11,
         {{.buffer = material->getBufferCoefficients()[currentFrame]->getData(),
           .offset = 0,
           .range = material->getBufferCoefficients()[currentFrame]->getSize()}}}};

    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor = {
        {1,
         {{.sampler = material->getBaseColor()[0]->getSampler()->getSampler(),
           .imageView = material->getBaseColor()[0]->getImageView()->getImageView(),
           .imageLayout = material->getBaseColor()[0]->getImageView()->getImage()->getImageLayout()}}},
        {2,
         {{.sampler = material->getNormal()[0]->getSampler()->getSampler(),
           .imageView = material->getNormal()[0]->getImageView()->getImageView(),
           .imageLayout = material->getNormal()[0]->getImageView()->getImage()->getImageLayout()}}},
        {3,
         {{.sampler = material->getMetallic()[0]->getSampler()->getSampler(),
           .imageView = material->getMetallic()[0]->getImageView()->getImageView(),
           .imageLayout = material->getMetallic()[0]->getImageView()->getImage()->getImageLayout()}}},
        {4,
         {{.sampler = material->getRoughness()[0]->getSampler()->getSampler(),
           .imageView = material->getRoughness()[0]->getImageView()->getImageView(),
           .imageLayout = material->getRoughness()[0]->getImageView()->getImage()->getImageLayout()}}},
        {5,
         {{.sampler = material->getOccluded()[0]->getSampler()->getSampler(),
           .imageView = material->getOccluded()[0]->getImageView()->getImageView(),
           .imageLayout = material->getOccluded()[0]->getImageView()->getImage()->getImageLayout()}}},
        {6,
         {{.sampler = material->getEmissive()[0]->getSampler()->getSampler(),
           .imageView = material->getEmissive()[0]->getImageView()->getImageView(),
           .imageLayout = material->getEmissive()[0]->getImageView()->getImage()->getImageLayout()}}},
        {7,
         {{.sampler = material->getDiffuseIBL()->getSampler()->getSampler(),
           .imageView = material->getDiffuseIBL()->getImageView()->getImageView(),
           .imageLayout = material->getDiffuseIBL()->getImageView()->getImage()->getImageLayout()}}},
        {8,
         {{.sampler = material->getSpecularIBL()->getSampler()->getSampler(),
           .imageView = material->getSpecularIBL()->getImageView()->getImageView(),
           .imageLayout = material->getSpecularIBL()->getImageView()->getImage()->getImageLayout()}}},
        {9,
         {{.sampler = material->getSpecularBRDF()->getSampler()->getSampler(),
           .imageView = material->getSpecularBRDF()->getImageView()->getImageView(),
           .imageLayout = material->getSpecularBRDF()->getImageView()->getImage()->getImageLayout()}}}};

    _descriptorSetPBR[i]->createCustom(currentFrame, bufferInfoColor, textureInfoColor);
  }
}

void Model3D::_updatePhongDescriptor() {
  int currentFrame = _engineState->getFrameInFlight();
  for (int i = 0; i < _materials.size(); i++) {
    std::shared_ptr<MaterialPhong> material = std::dynamic_pointer_cast<MaterialPhong>(_materials[i]);
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor = {
        {0,
         {{.buffer = _cameraUBOFull[currentFrame]->getData(),
           .offset = 0,
           .range = _cameraUBOFull[currentFrame]->getSize()}}},
        {4,
         {{.buffer = material->getBufferAlphaCutoff()[currentFrame]->getData(),
           .offset = 0,
           .range = material->getBufferAlphaCutoff()[currentFrame]->getSize()}}},
        {5,
         {{.buffer = material->getBufferCoefficients()[currentFrame]->getData(),
           .offset = 0,
           .range = material->getBufferCoefficients()[currentFrame]->getSize()}}}};
    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor = {
        {1,
         {{.sampler = material->getBaseColor()[0]->getSampler()->getSampler(),
           .imageView = material->getBaseColor()[0]->getImageView()->getImageView(),
           .imageLayout = material->getBaseColor()[0]->getImageView()->getImage()->getImageLayout()}}},
        {2,
         {{.sampler = material->getNormal()[0]->getSampler()->getSampler(),
           .imageView = material->getNormal()[0]->getImageView()->getImageView(),
           .imageLayout = material->getNormal()[0]->getImageView()->getImage()->getImageLayout()}}},
        {3,
         {{.sampler = material->getSpecular()[0]->getSampler()->getSampler(),
           .imageView = material->getSpecular()[0]->getImageView()->getImageView(),
           .imageLayout = material->getSpecular()[0]->getImageView()->getImage()->getImageLayout()}}}};

    _descriptorSetPhong[i]->createCustom(currentFrame, bufferInfoColor, textureInfoColor);
  }
}

void Model3D::_updateColorDescriptor() {
  int currentFrame = _engineState->getFrameInFlight();
  for (int i = 0; i < _materials.size(); i++) {
    std::shared_ptr<MaterialColor> material = std::dynamic_pointer_cast<MaterialColor>(_materials[i]);
    std::map<int, std::vector<VkDescriptorBufferInfo>> bufferInfoColor = {
        {0,
         {{.buffer = _cameraUBOFull[currentFrame]->getData(),
           .offset = 0,
           .range = _cameraUBOFull[currentFrame]->getSize()}}}};
    auto texture = _defaultMaterialColor->getBaseColor();
    if (material->getBaseColor().size() > 0) texture = material->getBaseColor();
    std::map<int, std::vector<VkDescriptorImageInfo>> textureInfoColor = {
        {1,
         {{.sampler = texture[0]->getSampler()->getSampler(),
           .imageView = texture[0]->getImageView()->getImageView(),
           .imageLayout = texture[0]->getImageView()->getImage()->getImageLayout()}}}};
    _descriptorSetColor[i]->createCustom(currentFrame, bufferInfoColor, textureInfoColor);
  }
}

void Model3D::setMaterial(std::vector<std::shared_ptr<MaterialColor>> materials) {
  for (int material = 0; material < materials.size(); material++) {
    if (material < _materials.size() && material < _descriptorSetColor.size())
      _materials[material]->unregisterUpdate(_descriptorSetColor[material]);

    if (_descriptorSetColor.size() <= material)
      _descriptorSetColor.push_back(std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                                    _descriptorSetLayoutColor, _engineState));
    materials[material]->registerUpdate(_descriptorSetColor[material], {{MaterialTexture::COLOR, 1}});
  }
  _materialType = MaterialType::COLOR;

  _materials.clear();
  for (auto& material : materials) {
    _materials.push_back(material);
  }
  for (int i = 0; i < _changedMaterial.size(); i++) {
    _changedMaterial[i] = true;
  }
}

void Model3D::setMaterial(std::vector<std::shared_ptr<MaterialPhong>> materials) {
  for (int material = 0; material < materials.size(); material++) {
    if (material < _materials.size() && material < _descriptorSetPhong.size())
      _materials[material]->unregisterUpdate(_descriptorSetPhong[material]);

    if (_descriptorSetPhong.size() <= material)
      _descriptorSetPhong.push_back(std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                                    _descriptorSetLayoutPhong, _engineState));
    materials[material]->registerUpdate(
        _descriptorSetPhong[material],
        {{MaterialTexture::COLOR, 1}, {MaterialTexture::NORMAL, 2}, {MaterialTexture::SPECULAR, 3}});
  }
  _materialType = MaterialType::PHONG;

  _materials.clear();
  for (auto& material : materials) {
    _materials.push_back(material);
  }
  for (int i = 0; i < _changedMaterial.size(); i++) {
    _changedMaterial[i] = true;
  }
}

void Model3D::setMaterial(std::vector<std::shared_ptr<MaterialPBR>> materials) {
  for (int material = 0; material < materials.size(); material++) {
    if (material < _materials.size() && material < _descriptorSetPBR.size())
      _materials[material]->unregisterUpdate(_descriptorSetPBR[material]);

    if (_descriptorSetPBR.size() <= material)
      _descriptorSetPBR.push_back(std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                                  _descriptorSetLayoutPBR, _engineState));
    materials[material]->registerUpdate(_descriptorSetPBR[material], {{MaterialTexture::COLOR, 1},
                                                                      {MaterialTexture::NORMAL, 2},
                                                                      {MaterialTexture::METALLIC, 3},
                                                                      {MaterialTexture::ROUGHNESS, 4},
                                                                      {MaterialTexture::OCCLUSION, 5},
                                                                      {MaterialTexture::EMISSIVE, 6},
                                                                      {MaterialTexture::IBL_DIFFUSE, 7},
                                                                      {MaterialTexture::IBL_SPECULAR, 8},
                                                                      {MaterialTexture::BRDF_SPECULAR, 9}});
  }
  _materialType = MaterialType::PBR;
  _materials.clear();
  for (auto& material : materials) {
    _materials.push_back(material);
  }
  for (int i = 0; i < _changedMaterial.size(); i++) {
    _changedMaterial[i] = true;
  }
}

void Model3D::setDrawType(DrawType drawType) { _drawType = drawType; }

MaterialType Model3D::getMaterialType() { return _materialType; }

DrawType Model3D::getDrawType() { return _drawType; }

std::shared_ptr<AABB> Model3D::getAABB() {
  std::shared_ptr<AABB> aabbTotal = std::make_shared<AABB>();
  for (auto& mesh : _meshes) {
    auto aabb = mesh->getAABB();
    aabbTotal->extend(aabb);
  }
  return aabbTotal;
}

void Model3D::setAnimation(std::shared_ptr<Animation> animation) {
  _animation = animation;
  _updateJointsDescriptor();
}

void Model3D::_drawNode(std::shared_ptr<CommandBuffer> commandBuffer,
                        std::shared_ptr<Pipeline> pipeline,
                        std::shared_ptr<Pipeline> pipelineCullOff,
                        std::shared_ptr<DescriptorSet> cameraDS,
                        std::vector<std::shared_ptr<Buffer>> cameraUBO,
                        glm::mat4 view,
                        glm::mat4 projection,
                        std::shared_ptr<NodeGLTF> node) {
  int currentFrame = _engineState->getFrameInFlight();
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
    BufferMVP cameraMVP{.model = getModel() * nodeMatrix, .view = view, .projection = projection};

    cameraUBO[currentFrame]->setData(&cameraMVP);

    auto pipelineLayout = pipeline->getDescriptorSetLayout();

    // normals and tangents
    auto normalTangentLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                            [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                              return info.first == std::string("normal");
                                            });
    if (normalTangentLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 0, 1,
                              &_descriptorSetNormalsMesh->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    // depth
    auto depthLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                    [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                      return info.first == std::string("depth");
                                    });
    if (depthLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 0, 1, &cameraDS->getDescriptorSets()[currentFrame], 0,
                              nullptr);
    }

    // joints
    auto jointLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                    [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                      return info.first == std::string("joints");
                                    });

    if (jointLayout != pipelineLayout.end()) {
      // if node->skin == -1 then use 0 index that contains identity matrix because of animation default behavior
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 1, 1,
                              &_descriptorSetJoints[std::max(0, node->skin)]->getDescriptorSets()[currentFrame], 0,
                              nullptr);
    }

    for (MeshPrimitive primitive : _meshes[node->mesh]->getPrimitives()) {
      if (primitive.indexCount > 0) {
        std::shared_ptr<Material> material = _defaultMaterialPhong;
        int materialIndex = 0;
        // Get the texture index for this primitive
        if (_materials.size() > 0) {
          material = _materials.front();
          if (primitive.materialIndex >= 0 && primitive.materialIndex < _materials.size()) {
            material = _materials[primitive.materialIndex];
            materialIndex = primitive.materialIndex;
          }
        }

        // color
        auto layoutColor = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                          return info.first == std::string("color");
                                        });
        if (layoutColor != pipelineLayout.end()) {
          vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipeline->getPipelineLayout(), 0, 1,
                                  &_descriptorSetColor[materialIndex]->getDescriptorSets()[currentFrame], 0, nullptr);
        }

        // phong
        auto layoutPhong = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                          return info.first == std::string("phong");
                                        });
        if (layoutPhong != pipelineLayout.end()) {
          vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipeline->getPipelineLayout(), 0, 1,
                                  &_descriptorSetPhong[materialIndex]->getDescriptorSets()[currentFrame], 0, nullptr);
        }

        // pbr
        auto layoutPBR = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                        return info.first == std::string("pbr");
                                      });
        if (layoutPBR != pipelineLayout.end()) {
          vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipeline->getPipelineLayout(), 0, 1,
                                  &_descriptorSetPBR[materialIndex]->getDescriptorSets()[currentFrame], 0, nullptr);
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

void Model3D::draw(std::shared_ptr<CommandBuffer> commandBuffer) {
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

  auto resolution = _engineState->getSettings()->getResolution();
  int currentFrame = _engineState->getFrameInFlight();

  if (_changedMaterial[currentFrame]) {
    switch (_materialType) {
      case MaterialType::COLOR:
        _updateColorDescriptor();
        break;
      case MaterialType::PHONG:
        _updatePhongDescriptor();
        break;
      case MaterialType::PBR:
        _updatePBRDescriptor();
        break;
    }
    _changedMaterial[currentFrame] = false;
  }

  VkViewport viewport{.x = 0.0f,
                      .y = static_cast<float>(std::get<1>(resolution)),
                      .width = static_cast<float>(std::get<0>(resolution)),
                      .height = static_cast<float>(-std::get<1>(resolution)),
                      .minDepth = 0.0f,
                      .maxDepth = 1.0f};
  vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{.offset = {0, 0}, .extent = VkExtent2D(std::get<0>(resolution), std::get<1>(resolution))};
  vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  if (pipeline->getPushConstants().find("constants") != pipeline->getPushConstants().end()) {
    FragmentPush pushConstants{.enableShadow = _enableShadow,
                               .enableLighting = _enableLighting,
                               .cameraPosition = _gameState->getCameraManager()->getCurrentCamera()->getEye()};
    auto info = pipeline->getPushConstants()["constants"];
    vkCmdPushConstants(commandBuffer->getCommandBuffer()[_engineState->getFrameInFlight()],
                       pipeline->getPipelineLayout(), info.stageFlags, info.offset, info.size, &pushConstants);
  }

  auto pipelineLayout = pipeline->getDescriptorSetLayout();

  // global Phong
  auto globalLayoutPhong = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                        [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                          return info.first == std::string("globalPhong");
                                        });
  if (globalLayoutPhong != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(),
        2, 1, &_gameState->getLightManager()->getDSGlobalPhong()->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  // global PBR
  auto globalLayoutPBR = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                        return info.first == std::string("globalPBR");
                                      });
  if (globalLayoutPBR != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(),
        2, 1, &_gameState->getLightManager()->getDSGlobalPBR()->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  // Render all nodes at top-level
  for (auto& node : _nodes) {
    _drawNode(commandBuffer, pipeline, pipelineCullOff, nullptr, _cameraUBOFull,
              _gameState->getCameraManager()->getCurrentCamera()->getView(),
              _gameState->getCameraManager()->getCurrentCamera()->getProjection(), node);
  }
}

void Model3D::drawShadow(LightType lightType, int lightIndex, int face, std::shared_ptr<CommandBuffer> commandBuffer) {
  if (_enableDepth == false) return;

  int currentFrame = _engineState->getFrameInFlight();
  auto pipeline = _pipelineDirectional;
  if (lightType == LightType::POINT) pipeline = _pipelinePoint;

  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->getPipeline());

  std::tuple<int, int> resolution;
  if (lightType == LightType::DIRECTIONAL) {
    resolution = _gameState->getLightManager()
                     ->getDirectionalShadows()[lightIndex]
                     ->getShadowMapTexture()[currentFrame]
                     ->getImageView()
                     ->getImage()
                     ->getResolution();
  }
  if (lightType == LightType::POINT) {
    resolution = _gameState->getLightManager()
                     ->getPointShadows()[lightIndex]
                     ->getShadowMapCubemap()[currentFrame]
                     ->getTexture()
                     ->getImageView()
                     ->getImage()
                     ->getResolution();
  }

  // Cube Maps have been specified to follow the RenderMan specification (for whatever reason),
  // and RenderMan assumes the images' origin being in the upper left so we don't need to swap anything
  VkViewport viewport{};
  if (lightType == LightType::DIRECTIONAL) {
    viewport = {.x = 0.f,
                .y = static_cast<float>(std::get<1>(resolution)),
                .width = static_cast<float>(std::get<0>(resolution)),
                .height = static_cast<float>(-std::get<1>(resolution))};
  } else if (lightType == LightType::POINT) {
    viewport = {.x = 0.f,
                .y = 0.f,
                .width = static_cast<float>(std::get<0>(resolution)),
                .height = static_cast<float>(std::get<1>(resolution))};
  }
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{.offset = {0, 0}, .extent = VkExtent2D(std::get<0>(resolution), std::get<1>(resolution))};
  vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  if (lightType == LightType::POINT) {
    if (pipeline->getPushConstants().find("constants") != pipeline->getPushConstants().end()) {
      FragmentPointLightPushDepth pushConstants;
      pushConstants.lightPosition =
          _gameState->getLightManager()->getPointLights()[lightIndex]->getCamera()->getPosition();
      // light camera
      pushConstants.far = _gameState->getLightManager()->getPointLights()[lightIndex]->getCamera()->getFar();
      auto info = pipeline->getPushConstants()["constants"];
      vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                         info.stageFlags, info.offset, info.size, &pushConstants);
    }
  }

  glm::mat4 view(1.f);
  glm::mat4 projection(1.f);
  int lightIndexTotal = lightIndex;
  if (lightType == LightType::DIRECTIONAL) {
    view = _gameState->getLightManager()->getDirectionalLights()[lightIndex]->getCamera()->getView();
    projection = _gameState->getLightManager()->getDirectionalLights()[lightIndex]->getCamera()->getProjection();
  }
  if (lightType == LightType::POINT) {
    lightIndexTotal += _engineState->getSettings()->getMaxDirectionalLights();
    view = _gameState->getLightManager()->getPointLights()[lightIndex]->getCamera()->getView(face);
    projection = _gameState->getLightManager()->getPointLights()[lightIndex]->getCamera()->getProjection();
  }
  // Render all nodes at top-level
  for (auto& node : _nodes) {
    _drawNode(commandBuffer, pipeline, pipeline, _descriptorSetCameraDepth[lightIndexTotal][face],
              _cameraUBODepth[lightIndexTotal][face], view, projection, node);
  }
}