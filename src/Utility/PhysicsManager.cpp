#include "Utility/PhysicsManager.h"

PhysicsManager::PhysicsManager() {
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
}

JPH::BodyInterface& PhysicsManager::getBodyInterface() { return _physicsSystem.GetBodyInterface(); }

JPH::PhysicsSystem& PhysicsManager::getPhysicsSystem() { return _physicsSystem; }

void PhysicsManager::update() {
  _physicsSystem.Update(_deltaTime, _collisionSteps, _tempAllocator.get(), _jobSystem.get());
}

PhysicsManager::~PhysicsManager() {}