#include <iostream>
#include <chrono>
#include <future>
#include "Main.h"
#include <glm/gtx/matrix_decompose.hpp>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

using namespace JPH::literals;

// Layer that objects can be in, determines which other objects it can collide with
// Typically you at least want to have 1 layer for moving bodies and 1 layer for static bodies, but you can have more
// layers if you want. E.g. you could have a layer for high detail collision (which is not used by the physics
// simulation but only if you do collision testing).
namespace Layers {
static constexpr JPH::ObjectLayer NON_MOVING = 0;
static constexpr JPH::ObjectLayer MOVING = 1;
static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
};  // namespace Layers

/// Class that determines if two object layers can collide
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
 public:
  virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
    switch (inObject1) {
      case Layers::NON_MOVING:
        return inObject2 == Layers::MOVING;  // Non moving only collides with moving
      case Layers::MOVING:
        return true;  // Moving collides with everything
      default:
        JPH_ASSERT(false);
        return false;
    }
  }
};

// Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
// a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
// You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
// many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
// your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
namespace BroadPhaseLayers {
static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
static constexpr JPH::BroadPhaseLayer MOVING(1);
static constexpr JPH::uint NUM_LAYERS(2);
};  // namespace BroadPhaseLayers

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
 public:
  BPLayerInterfaceImpl() {
    // Create a mapping table from object to broad phase layer
    mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
    mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
  }

  virtual JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }

  virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
    JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
    return mObjectToBroadPhase[inLayer];
  }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
    switch ((JPH::BroadPhaseLayer::Type)inLayer) {
      case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
        return "NON_MOVING";
      case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
        return "MOVING";
      default:
        JPH_ASSERT(false);
        return "INVALID";
    }
  }
#endif  // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

 private:
  JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
 public:
  virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
    switch (inLayer1) {
      case Layers::NON_MOVING:
        return inLayer2 == BroadPhaseLayers::MOVING;
      case Layers::MOVING:
        return true;
      default:
        JPH_ASSERT(false);
        return false;
    }
  }
};

// An example contact listener
class MyContactListener : public JPH::ContactListener {
 public:
  // See: ContactListener
  virtual JPH::ValidateResult OnContactValidate(const JPH::Body& inBody1,
                                                const JPH::Body& inBody2,
                                                JPH::RVec3Arg inBaseOffset,
                                                const JPH::CollideShapeResult& inCollisionResult) override {
    std::cout << "Contact validate callback" << std::endl;

    // Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
    return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
  }

  virtual void OnContactAdded(const JPH::Body& inBody1,
                              const JPH::Body& inBody2,
                              const JPH::ContactManifold& inManifold,
                              JPH::ContactSettings& ioSettings) override {
    std::cout << "A contact was added" << std::endl;
  }

  virtual void OnContactPersisted(const JPH::Body& inBody1,
                                  const JPH::Body& inBody2,
                                  const JPH::ContactManifold& inManifold,
                                  JPH::ContactSettings& ioSettings) override {
    std::cout << "A contact was persisted" << std::endl;
  }

  virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override {
    std::cout << "A contact was removed" << std::endl;
  }
};

// An example activation listener
class MyBodyActivationListener : public JPH::BodyActivationListener {
 public:
  virtual void OnBodyActivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override {
    // std::cout << "A body got activated" << std::endl;
  }

  virtual void OnBodyDeactivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override {
    // std::cout << "A body went to sleep" << std::endl;
  }
};

glm::vec3 getPosition(std::shared_ptr<Drawable> drawable) {
  glm::vec3 scale;
  glm::quat rotation;
  glm::vec3 translation;
  glm::vec3 skew;
  glm::vec4 perspective;
  glm::decompose(drawable->getModel(), scale, rotation, translation, skew, perspective);
  return translation;
}

float getHeight(std::tuple<std::shared_ptr<uint8_t[]>, std::tuple<int, int, int>> heightmap, glm::vec3 position) {
  auto [data, dimension] = heightmap;
  auto [width, height, channels] = dimension;

  float x = position.x + (width - 1) / 2.f;
  float z = position.z + (height - 1) / 2.f;
  int xIntegral = x;
  int zIntegral = z;
  // 0 1
  // 2 3

  // channels 2, width 4, (2, 1) = 2 * 2 + 1 * 4 * 2 = 12
  // 0 0 1 1 2 2 3 3
  // 4 4 5 5 6 6 7 7
  int index0 = xIntegral * channels + zIntegral * (width * channels);
  int index1 = (xIntegral + 1) * channels + zIntegral * (width * channels);
  int index2 = xIntegral * channels + (zIntegral + 1) * (width * channels);
  int index3 = (xIntegral + 1) * channels + (zIntegral + 1) * (width * channels);
  float sample0 = data[index0] / 255.f;
  float sample1 = data[index1] / 255.f;
  float sample2 = data[index2] / 255.f;
  float sample3 = data[index3] / 255.f;
  float fxy1 = sample0 + (x - xIntegral) * (sample1 - sample0);
  float fxy2 = sample2 + (x - xIntegral) * (sample3 - sample2);
  float sample = fxy1 + (z - zIntegral) * (fxy2 - fxy1);
  return sample * 64.f - 16.f;
}

void InputHandler::setMoveCallback(std::function<void(glm::vec3)> callback) { _callback = callback; }

InputHandler::InputHandler(std::shared_ptr<Core> core) { _core = core; }

void InputHandler::cursorNotify(float xPos, float yPos) {}

void InputHandler::mouseNotify(int button, int action, int mods) {}

void InputHandler::keyNotify(int key, int scancode, int action, int mods) {
#ifndef __ANDROID__
  if ((action == GLFW_RELEASE && key == GLFW_KEY_C)) {
    if (_cursorEnabled) {
      _core->getState()->getInput()->showCursor(false);
      _cursorEnabled = false;
    } else {
      _core->getState()->getInput()->showCursor(true);
      _cursorEnabled = true;
    }
  }
  std::optional<glm::vec3> shift = std::nullopt;
  if (action == GLFW_PRESS && key == GLFW_KEY_UP) {
    shift = glm::vec3(0.f, 0.f, -1.f);
  }
  if (action == GLFW_PRESS && key == GLFW_KEY_DOWN) {
    shift = glm::vec3(0.f, 0.f, 1.f);
  }
  if (action == GLFW_PRESS && key == GLFW_KEY_LEFT) {
    shift = glm::vec3(-1.f, 0.f, 0.f);
  }
  if (action == GLFW_PRESS && key == GLFW_KEY_RIGHT) {
    shift = glm::vec3(1.f, 0.f, 0.f);
  }

  if (shift) {
    _callback(shift.value());
  }
#endif
}

void InputHandler::charNotify(unsigned int code) {}

void InputHandler::scrollNotify(double xOffset, double yOffset) {}

JPH::Array<JPH::uint8> ReadData(const char* inFileName) {
  JPH::Array<JPH::uint8> data;
  std::ifstream input(inFileName, std::ios::binary);
  input.seekg(0, std::ios_base::end);
  std::ifstream::pos_type length = input.tellg();
  input.seekg(0, std::ios_base::beg);
  data.resize(size_t(length));
  input.read((char*)&data[0], length);
  return data;
}

std::jthread _physicSimulate;
JPH::PhysicsSystem _physicsSystem;
std::shared_ptr<JPH::TempAllocatorImpl> _tempAllocator;
std::shared_ptr<JPH::JobSystemThreadPool> _jobSystem;
// Create mapping table from object layer to broadphase layer
// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
BPLayerInterfaceImpl _broadPhaseLayerInterface;
// Create class that filters object vs broadphase layers
// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
ObjectVsBroadPhaseLayerFilterImpl _objectVsBroadphaseLayerFilter;
// Create class that filters object vs object layers
// Note: As this is an interface, PhysicsSystem will take a reference to this so this instance needs to stay alive!
ObjectLayerPairFilterImpl _objectVsObjectLayerFilter;
MyBodyActivationListener _bodyActivationListener;
MyContactListener _contactListener;
JPH::BodyID _boxID;
std::ofstream ofp("test.txt");

JPH::Array<float> mTerrain;
JPH::uint mTerrainSize;
JPH::Vec3 mTerrainOffset;
JPH::Vec3 mTerrainScale;
JPH::RefConst<JPH::HeightFieldShape> mHeightField;

Main::Main() {
  int mipMapLevels = 8;
  auto settings = std::make_shared<Settings>();
  settings->setName("Terrain");
  settings->setClearColor({0.01f, 0.01f, 0.01f, 1.f});
  // TODO: fullscreen if resolution is {0, 0}
  // TODO: validation layers complain if resolution is {2560, 1600}
  settings->setResolution(std::tuple{1920, 1080});
  // for HDR, linear 16 bit per channel to represent values outside of 0-1 range (UNORM - float [0, 1], SFLOAT - float)
  // https://registry.khronos.org/vulkan/specs/1.1/html/vkspec.html#_identification_of_formats
  settings->setGraphicColorFormat(VK_FORMAT_R32G32B32A32_SFLOAT);
  settings->setSwapchainColorFormat(VK_FORMAT_B8G8R8A8_UNORM);
  // SRGB the same as UNORM but + gamma conversion out of box (!)
  settings->setLoadTextureColorFormat(VK_FORMAT_R8G8B8A8_SRGB);
  settings->setLoadTextureAuxilaryFormat(VK_FORMAT_R8G8B8A8_UNORM);
  settings->setAnisotropicSamples(4);
  settings->setDepthFormat(VK_FORMAT_D32_SFLOAT);
  settings->setMaxFramesInFlight(2);
  settings->setThreadsInPool(6);
  settings->setDesiredFPS(60);

  _core = std::make_shared<Core>(settings);
  _core->initialize();
  _core->startRecording();
  _camera = std::make_shared<CameraFly>(_core->getState());
  _camera->setProjectionParameters(60.f, 0.1f, 100.f);
  _core->getState()->getInput()->subscribe(std::dynamic_pointer_cast<InputSubscriber>(_camera));
  _inputHandler = std::make_shared<InputHandler>(_core);
  _core->getState()->getInput()->subscribe(std::dynamic_pointer_cast<InputSubscriber>(_inputHandler));
  _core->setCamera(_camera);

  _pointLightVertical = _core->createPointLight(settings->getDepthResolution());
  _pointLightVertical->setColor(glm::vec3(1.f, 1.f, 1.f));
  _pointLightHorizontal = _core->createPointLight(settings->getDepthResolution());
  _pointLightHorizontal->setColor(glm::vec3(1.f, 1.f, 1.f));
  _directionalLight = _core->createDirectionalLight(settings->getDepthResolution());
  _directionalLight->setColor(glm::vec3(1.f, 1.f, 1.f));
  _directionalLight->setPosition(glm::vec3(0.f, 20.f, 0.f));
  // TODO: rename setCenter to lookAt
  //  looking to (0.f, 0.f, 0.f) with up vector (0.f, 0.f, -1.f)
  _directionalLight->setCenter({0.f, 0.f, 0.f});
  _directionalLight->setUp({0.f, 0.f, -1.f});
  auto ambientLight = _core->createAmbientLight();
  ambientLight->setColor({0.1f, 0.1f, 0.1f});

  // cube colored light
  _cubeColoredLightVertical = _core->createShape3D(ShapeType::CUBE);
  _cubeColoredLightVertical->getMesh()->setColor(
      std::vector{_cubeColoredLightVertical->getMesh()->getVertexData().size(), glm::vec3(1.f, 1.f, 1.f)},
      _core->getCommandBufferApplication());
  _core->addDrawable(_cubeColoredLightVertical);

  _cubeColoredLightHorizontal = _core->createShape3D(ShapeType::CUBE);
  _cubeColoredLightHorizontal->getMesh()->setColor(
      std::vector{_cubeColoredLightHorizontal->getMesh()->getVertexData().size(), glm::vec3(1.f, 1.f, 1.f)},
      _core->getCommandBufferApplication());
  _core->addDrawable(_cubeColoredLightHorizontal);

  auto cubeColoredLightDirectional = _core->createShape3D(ShapeType::CUBE);
  cubeColoredLightDirectional->getMesh()->setColor(
      std::vector{cubeColoredLightDirectional->getMesh()->getVertexData().size(), glm::vec3(1.f, 1.f, 1.f)},
      _core->getCommandBufferApplication());
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 20.f, 0.f));
    model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f));
    cubeColoredLightDirectional->setModel(model);
  }
  _core->addDrawable(cubeColoredLightDirectional);

  auto heightmapCPU = _core->loadImageCPU("../assets/heightmap.png");

  auto heightmapFloat = _core->getResourceManager()->loadImageCPU<float>("../assets/heightmap.png");

  // Register allocation hook. In this example we'll just let Jolt use malloc / free but you can override these if you
  // want (see Memory.h). This needs to be done before any other Jolt function is called.
  JPH::RegisterDefaultAllocator();

  // Create a factory, this class is responsible for creating instances of classes based on their name or hash and is
  // mainly used for deserialization of saved data. It is not directly used in this example but still required.
  JPH::Factory::sInstance = new JPH::Factory();

  // Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
  // If you have your own custom shape types you probably need to register their handlers with the CollisionDispatch
  // before calling this function. If you implement your own default material (PhysicsMaterial::sDefault) make sure to
  // initialize it before this function or else this function will create one for you.
  JPH::RegisterTypes();

  // We need a temp allocator for temporary allocations during the physics update. We're
  // pre-allocating 10 MB to avoid having to do allocations during the physics update.
  // B.t.w. 10 MB is way too much for this example but it is a typical value you can use.
  // If you don't want to pre-allocate you can also use TempAllocatorMalloc to fall back to
  // malloc / free.
  _tempAllocator = std::make_shared<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
  // We need a job system that will execute physics jobs on multiple threads. Typically
  // you would implement the JobSystem interface yourself and let Jolt Physics run on top
  // of your own job scheduler. JobSystemThreadPool is an example implementation.
  _jobSystem = std::make_shared<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
                                                          JPH::thread::hardware_concurrency() - 1);

  // This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an
  // error. Note: This value is low because this is a simple test. For a real project use something in the order of
  // 65536.
  const int maxBodies = 1024;
  // This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the
  // default settings.
  const int numBodyMutexes = 0;
  // This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
  // body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make this
  // buffer too small the queue will fill up and the broad phase jobs will start to do narrow phase work. This is
  // slightly less efficient. Note: This value is low because this is a simple test. For a real project use something in
  // the order of 65536.
  const int maxBodyPairs = 1024;
  // This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are
  // detected than this number then these contacts will be ignored and bodies will start interpenetrating / fall through
  // the world. Note: This value is low because this is a simple test. For a real project use something in the order of
  // 10240.
  const int maxContactConstraints = 1024;

  _physicsSystem.Init(maxBodies, numBodyMutexes, maxBodyPairs, maxContactConstraints, _broadPhaseLayerInterface,
                      _objectVsBroadphaseLayerFilter, _objectVsObjectLayerFilter);

  // A body activation listener gets notified when bodies activate and go to sleep
  // Note that this is called from a job so whatever you do here needs to be thread safe.
  // Registering one is entirely optional.
  _physicsSystem.SetBodyActivationListener(&_bodyActivationListener);

  // A contact listener gets notified when bodies (are about to) collide, and when they separate again.
  // Note that this is called from a job so whatever you do here needs to be thread safe.
  // Registering one is entirely optional.
  _physicsSystem.SetContactListener(&_contactListener);

  {
    const int n = 256;
    // Get height samples
    JPH::Array<JPH::uint8> data = ReadData("heightmap.bin");
    mTerrainSize = n;
    mTerrain.resize(n * n);
    memcpy(mTerrain.data(), data.data(), n * n * sizeof(float));

    // Determine scale and offset
    mTerrainOffset = JPH::Vec3(0.f, -16.0f, 0.f);
    mTerrainScale = JPH::Vec3(1.f, 64.f, 1.f);
  }

  // The main way to interact with the bodies in the physics system is through the body interface. There is a locking
  // and a non-locking variant of this. We're going to use the locking version (even though we're not planning to access
  // bodies from multiple threads)
  JPH::BodyInterface& bodyInterface = _physicsSystem.GetBodyInterface();

  // Create height field
  JPH::HeightFieldShapeSettings settingsTerrain(mTerrain.data(), mTerrainOffset, mTerrainScale, mTerrainSize);
  mHeightField = JPH::StaticCast<JPH::HeightFieldShape>(settingsTerrain.Create().Get());

  JPH::Body* terrain = bodyInterface.CreateBody(JPH::BodyCreationSettings(
      mHeightField, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), JPH::EMotionType::Static, Layers::NON_MOVING));
  bodyInterface.AddBody(terrain->GetID(), JPH::EActivation::DontActivate);
  bodyInterface.SetPosition(terrain->GetID(), JPH::Vec3(-128, 0, -128), JPH::EActivation::DontActivate);

  JPH::BodyCreationSettings boxSettings(new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f)), JPH::RVec3(0.0, 50.0, 0.0),
                                        JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, Layers::MOVING);
  _boxID = bodyInterface.CreateAndAddBody(boxSettings, JPH::EActivation::Activate);

  _cubePlayer = _core->createShape3D(ShapeType::CUBE);
  _cubePlayer->setModel(glm::translate(glm::mat4(1.f), glm::vec3(0.f, 2.f, 0.f)));
  _cubePlayer->getMesh()->setColor(
      std::vector{_cubePlayer->getMesh()->getVertexData().size(), glm::vec3(0.f, 0.f, 1.f)},
      _core->getCommandBufferApplication());
  _core->addDrawable(_cubePlayer);
  auto callbackPosition = [player = _cubePlayer, heightmap = heightmapCPU](glm::vec3 shift) {
    glm::vec3 position = getPosition(player);
    position += shift;
    auto height = getHeight(heightmap, position);
    position.y = height;
    auto model = glm::translate(glm::mat4(1.f), position);
    player->setModel(model);
  };
  _inputHandler->setMoveCallback(callbackPosition);

  // Add it to the world
  {
    auto tile0 = _core->createTexture("../assets/desert/albedo.png", settings->getLoadTextureColorFormat(),
                                      mipMapLevels);
    auto tile1 = _core->createTexture("../assets/rock/albedo.png", settings->getLoadTextureColorFormat(), mipMapLevels);
    auto tile2 = _core->createTexture("../assets/grass/albedo.png", settings->getLoadTextureColorFormat(),
                                      mipMapLevels);
    auto tile3 = _core->createTexture("../assets/ground/albedo.png", settings->getLoadTextureColorFormat(),
                                      mipMapLevels);

    _terrainColor = _core->createTerrain("../assets/heightmap.png", std::pair{12, 12});
    auto materialColor = _core->createMaterialColor(MaterialTarget::TERRAIN);
    materialColor->setBaseColor({tile0, tile1, tile2, tile3});
    _terrainColor->setMaterial(materialColor);
    _core->addDrawable(_terrainColor);
  }

  /*{
    auto [data, size] = _core->loadImageCPU("../assets/heightmap.png");
    auto [w, h, c] = size;
    std::vector<float> transformed;
    for (int i = 0; i < w * h * c; i += c) {
      transformed.push_back((float) data[i] / 255.f);
    }
    std::ofstream ofp("heightmap.bin", std::ios::out | std::ios::binary);
    for (int i = 0; i < w * h; i++) {
      ofp.write(reinterpret_cast<const char*>(transformed.data() + i), sizeof(float));
    }
    ofp.close();
  }*/

  _core->endRecording();

  _core->registerUpdate(std::bind(&Main::update, this));
  // can be lambda passed that calls reset
  _core->registerReset(std::bind(&Main::reset, this, std::placeholders::_1, std::placeholders::_2));
}

void Main::update() {
  static float i = 0;
  // update light position
  float radius = 15.f;
  static float angleHorizontal = 90.f;
  glm::vec3 lightPositionHorizontal = glm::vec3(radius * cos(glm::radians(angleHorizontal)), radius,
                                                radius * sin(glm::radians(angleHorizontal)));
  static float angleVertical = 0.f;
  glm::vec3 lightPositionVertical = glm::vec3(0.f, radius * sin(glm::radians(angleVertical)),
                                              radius * cos(glm::radians(angleVertical)));

  _pointLightVertical->setPosition(lightPositionVertical);
  {
    auto model = glm::translate(glm::mat4(1.f), lightPositionVertical);
    model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f));
    _cubeColoredLightVertical->setModel(model);
  }
  _pointLightHorizontal->setPosition(lightPositionHorizontal);
  {
    auto model = glm::translate(glm::mat4(1.f), lightPositionHorizontal);
    model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f));
    _cubeColoredLightHorizontal->setModel(model);
  }

  i += 0.1f;
  angleHorizontal += 0.05f;
  angleVertical += 0.1f;

  auto [FPSLimited, FPSReal] = _core->getFPS();
  auto [widthScreen, heightScreen] = _core->getState()->getSettings()->getResolution();
  _core->getGUI()->startWindow("Terrain", {20, 20}, {widthScreen / 10, heightScreen / 10});
  if (_core->getGUI()->startTree("Info")) {
    _core->getGUI()->drawText({"Limited FPS: " + std::to_string(FPSLimited)});
    _core->getGUI()->drawText({"Maximum FPS: " + std::to_string(FPSReal)});
    _core->getGUI()->drawText({"Press 'c' to turn cursor on/off"});
    _core->getGUI()->endTree();
  }
  auto eye = _camera->getEye();
  auto direction = _camera->getDirection();
  if (_core->getGUI()->startTree("Coordinates")) {
    _core->getGUI()->drawText({std::string("eye x: ") + std::format("{:.2f}", eye.x),
                               std::string("eye y: ") + std::format("{:.2f}", eye.y),
                               std::string("eye z: ") + std::format("{:.2f}", eye.z)});
    auto model = _cubePlayer->getModel();
    _core->getGUI()->drawText({std::string("player x: ") + std::format("{:.6f}", model[3][0]),
                               std::string("player y: ") + std::format("{:.6f}", model[3][1]),
                               std::string("player z: ") + std::format("{:.6f}", model[3][2])});
    _core->getGUI()->endTree();
  }
  _core->getGUI()->endWindow();

  const float deltaTime = 1.0f / 60.0f;
  JPH::BodyInterface& bodyInterface = _physicsSystem.GetBodyInterface();
  // Next step
  // Output current position and velocity of the sphere
  JPH::RVec3 position = bodyInterface.GetCenterOfMassPosition(_boxID);
  JPH::Vec3 velocity = bodyInterface.GetLinearVelocity(_boxID);
  ofp << ": Position = (" << position.GetX() << ", " << position.GetY() << ", " << position.GetZ() << "), Velocity = ("
      << velocity.GetX() << ", " << velocity.GetY() << ", " << velocity.GetZ() << ")" << std::endl;

  _cubePlayer->setModel(glm::translate(glm::mat4(1.f), glm::vec3(position.GetX(), position.GetY(), position.GetZ())));
  // If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep
  // the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
  const int collisionSteps = 1;

  // Step the world
  _physicsSystem.Update(deltaTime, collisionSteps, _tempAllocator.get(), _jobSystem.get());
}

void Main::reset(int width, int height) { _camera->setAspect((float)width / (float)height); }

void Main::start() { _core->draw(); }

int main() {
  try {
    auto main = std::make_shared<Main>();
    main->start();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}