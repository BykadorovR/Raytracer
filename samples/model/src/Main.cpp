#include <iostream>
#include <chrono>
#include <future>
#include "Main.h"
#include "Model.h"

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
#endif
}

void InputHandler::charNotify(unsigned int code) {}

void InputHandler::scrollNotify(double xOffset, double yOffset) {}

Main::Main() {
  int mipMapLevels = 4;
  auto settings = std::make_shared<Settings>();
  settings->setName("Model");
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
  settings->setAnisotropicSamples(0);
  settings->setDepthFormat(VK_FORMAT_D32_SFLOAT);
  settings->setMaxFramesInFlight(2);
  settings->setThreadsInPool(6);
  settings->setDesiredFPS(1000);

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

  // cube colored light
  _cubeColoredLightVertical = _core->createShape3D(ShapeType::CUBE, VK_CULL_MODE_BACK_BIT);
  _cubeColoredLightVertical->getMesh()->setColor(
      std::vector{_cubeColoredLightVertical->getMesh()->getVertexData().size(), glm::vec3(1.f, 1.f, 1.f)},
      _core->getCommandBufferTransfer());
  _core->addDrawable(_cubeColoredLightVertical);

  _cubeColoredLightHorizontal = _core->createShape3D(ShapeType::CUBE, VK_CULL_MODE_BACK_BIT);
  ;
  _cubeColoredLightHorizontal->getMesh()->setColor(
      std::vector{_cubeColoredLightHorizontal->getMesh()->getVertexData().size(), glm::vec3(1.f, 1.f, 1.f)},
      _core->getCommandBufferTransfer());
  _core->addDrawable(_cubeColoredLightHorizontal);

  auto cubeColoredLightDirectional = _core->createShape3D(ShapeType::CUBE, VK_CULL_MODE_BACK_BIT);
  cubeColoredLightDirectional->getMesh()->setColor(
      std::vector{cubeColoredLightDirectional->getMesh()->getVertexData().size(), glm::vec3(1.f, 1.f, 1.f)},
      _core->getCommandBufferTransfer());
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 20.f, 0.f));
    model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f));
    cubeColoredLightDirectional->setModel(model);
  }
  _core->addDrawable(cubeColoredLightDirectional);

  auto fillMaterialPhong = [core = _core](std::shared_ptr<MaterialPhong> material) {
    if (material->getBaseColor().size() == 0) material->setBaseColor({core->getResourceManager()->getTextureOne()});
    if (material->getNormal().size() == 0) material->setNormal({core->getResourceManager()->getTextureZero()});
    if (material->getSpecular().size() == 0) material->setSpecular({core->getResourceManager()->getTextureZero()});
  };

  auto fillMaterialPBR = [core = _core](std::shared_ptr<MaterialPBR> material) {
    if (material->getBaseColor().size() == 0) material->setBaseColor({core->getResourceManager()->getTextureOne()});
    if (material->getNormal().size() == 0) material->setNormal({core->getResourceManager()->getTextureZero()});
    if (material->getMetallic().size() == 0) material->setMetallic({core->getResourceManager()->getTextureZero()});
    if (material->getRoughness().size() == 0) material->setRoughness({core->getResourceManager()->getTextureZero()});
    if (material->getOccluded().size() == 0) material->setOccluded({core->getResourceManager()->getTextureZero()});
    if (material->getEmissive().size() == 0) material->setEmissive({core->getResourceManager()->getTextureZero()});
    material->setDiffuseIBL(core->getResourceManager()->getCubemapZero()->getTexture());
    material->setSpecularIBL(core->getResourceManager()->getCubemapZero()->getTexture(),
                             core->getResourceManager()->getTextureZero());
  };
  // auto gltfModelBox = _core->createModelGLTF("assets/Box/Box.gltf");
  //{
  //   auto modelBox = _core->createModel3D(gltfModelBox);
  //   auto materialModelBox = gltfModelBox->getMaterialsPBR();
  //   for (auto& material : materialModelBox) {
  //     fillMaterialPBR(material);
  //   }
  //   modelBox->setMaterial(materialModelBox);
  //   {
  //     auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 3.f, -3.f));
  //     modelBox->setModel(model);
  //   }

  //  _core->addDrawable(modelBox);
  //}

  //// draw simple textured model Phong
  // auto gltfModelAvocado = _core->createModelGLTF("../assets/Avocado/Avocado.gltf");
  //{
  //   auto modelAvocado = _core->createModel3D(gltfModelAvocado);
  //   auto materialModelAvocado = gltfModelAvocado->getMaterialsPhong();
  //   for (auto& material : materialModelAvocado) {
  //     fillMaterialPhong(material);
  //   }
  //   modelAvocado->setMaterial(materialModelAvocado);
  //   {
  //     auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -3.f));
  //     model = glm::scale(model, glm::vec3(15.f, 15.f, 15.f));
  //     modelAvocado->setModel(model);
  //   }

  //  _core->addDrawable(modelAvocado);
  //}
  //// draw simple textured model PBR
  //{
  //  auto modelAvocado = _core->createModel3D(gltfModelAvocado);
  //  auto materialModelAvocado = gltfModelAvocado->getMaterialsPBR();
  //  for (auto& material : materialModelAvocado) {
  //    fillMaterialPBR(material);
  //  }
  //  modelAvocado->setMaterial(materialModelAvocado);
  //  {
  //    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -1.f, -3.f));
  //    model = glm::scale(model, glm::vec3(15.f, 15.f, 15.f));
  //    modelAvocado->setModel(model);
  //  }
  //  _core->addDrawable(modelAvocado);
  //}

  //// draw advanced textured model Phong
  // auto gltfModelBottle = _core->createModelGLTF("../assets/WaterBottle/WaterBottle.gltf");
  //{
  //   auto modelBottle = _core->createModel3D(gltfModelBottle);
  //   auto materialModelBottle = gltfModelBottle->getMaterialsPhong();
  //   for (auto& material : materialModelBottle) {
  //     fillMaterialPhong(material);
  //   }
  //   modelBottle->setMaterial(materialModelBottle);
  //   {
  //     auto model = glm::translate(glm::mat4(1.f), glm::vec3(2.f, 0.f, -3.f));
  //     model = glm::scale(model, glm::vec3(3.f, 3.f, 3.f));
  //     modelBottle->setModel(model);
  //   }
  //   _core->addDrawable(modelBottle);
  // }

  //// draw advanced textured model PBR
  //{
  //  auto modelBottle = _core->createModel3D(gltfModelBottle);
  //  auto materialModelBottle = gltfModelBottle->getMaterialsPBR();
  //  for (auto& material : materialModelBottle) {
  //    fillMaterialPBR(material);
  //  }
  //  modelBottle->setMaterial(materialModelBottle);
  //  {
  //    auto model = glm::translate(glm::mat4(1.f), glm::vec3(2.f, -1.f, -3.f));
  //    model = glm::scale(model, glm::vec3(3.f, 3.f, 3.f));
  //    modelBottle->setModel(model);
  //  }
  //  _core->addDrawable(modelBottle);
  //}
  //// draw textured model without lighting model
  //{
  //  auto modelBottle = _core->createModel3D(gltfModelBottle);
  //  auto materialModelBottle = gltfModelBottle->getMaterialsColor();
  //  modelBottle->setMaterial(materialModelBottle);
  //  {
  //    auto model = glm::translate(glm::mat4(1.f), glm::vec3(2.f, -2.f, -3.f));
  //    model = glm::scale(model, glm::vec3(3.f, 3.f, 3.f));
  //    modelBottle->setModel(model);
  //  }
  //  _core->addDrawable(modelBottle);
  //}
  //// draw model without material
  //{
  //  auto modelBottle = _core->createModel3D(gltfModelBottle);
  //  {
  //    auto model = glm::translate(glm::mat4(1.f), glm::vec3(2.f, 1.f, -3.f));
  //    model = glm::scale(model, glm::vec3(3.f, 3.f, 3.f));
  //    modelBottle->setModel(model);
  //  }
  //  _core->addDrawable(modelBottle);
  //}
  //// draw material normal
  //{
  //  auto modelBottle = _core->createModel3D(gltfModelBottle);
  //  modelBottle->setDrawType(DrawType::NORMAL);
  //  {
  //    auto model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 1.f, -3.f));
  //    model = glm::scale(model, glm::vec3(3.f, 3.f, 3.f));
  //    modelBottle->setModel(model);
  //  }
  //  _core->addDrawable(modelBottle);
  //}
  //// draw wireframe
  //{
  //  auto modelBottle = _core->createModel3D(gltfModelBottle);
  //  modelBottle->setDrawType(DrawType::WIREFRAME);
  //  {
  //    auto model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 0.f, -3.f));
  //    model = glm::scale(model, glm::vec3(3.f, 3.f, 3.f));
  //    modelBottle->setModel(model);
  //  }
  //  _core->addDrawable(modelBottle);
  //}
  // draw skeletal textured model with multiple animations
  auto gltfModelFish = _core->createModelGLTF("../assets/Fish/scene.gltf");
  {
    auto modelFish = _core->createModel3D(gltfModelFish);
    auto materialModelFish = gltfModelFish->getMaterialsPhong();
    for (auto& material : materialModelFish) {
      fillMaterialPhong(material);
    }
    modelFish->setMaterial(materialModelFish);
    _animationFish = _core->createAnimation(gltfModelFish);
    _animationFish->setAnimation("swim");
    // set animation to model, so joints will be passed to shader
    modelFish->setAnimation(_animationFish);
    // register to play animation, if don't call, there will not be any animation,
    // even model can disappear because of zero start weights
    {
      auto model = glm::translate(glm::mat4(1.f), glm::vec3(-2.f, -1.f, -3.f));
      model = glm::scale(model, glm::vec3(5.f, 5.f, 5.f));
      modelFish->setModel(model);
    }
    _core->addDrawable(modelFish);
  }

  //// draw skeletal dancing model with one animation
  //{
  //  auto gltfModelDancing = _core->createModelGLTF("../assets/BrainStem/BrainStem.gltf");
  //  auto modelDancing = _core->createModel3D(gltfModelDancing);
  //  auto materialModelDancing = gltfModelDancing->getMaterialsPBR();
  //  for (auto& material : materialModelDancing) {
  //    fillMaterialPBR(material);
  //  }
  //  modelDancing->setMaterial(materialModelDancing);
  //  auto animationDancing = _core->createAnimation(gltfModelDancing);
  //  // set animation to model, so joints will be passed to shader
  //  modelDancing->setAnimation(animationDancing);
  //  {
  //    auto model = glm::translate(glm::mat4(1.f), glm::vec3(-4.f, -1.f, -3.f));
  //    model = glm::scale(model, glm::vec3(1.f, 1.f, 1.f));
  //    modelDancing->setModel(model);
  //  }
  //  _core->addDrawable(modelDancing);
  //}

  //// draw skeletal walking model with one animation
  //{
  //  auto gltfModelWalking = _core->getResourceManager()->loadModel("../assets/CesiumMan/CesiumMan.gltf");
  //  auto modelWalking = _core->createModel3D(gltfModelWalking);
  //  auto materialModelWalking = gltfModelWalking->getMaterialsPhong();
  //  for (auto& material : materialModelWalking) {
  //    fillMaterialPhong(material);
  //  }
  //  modelWalking->setMaterial(materialModelWalking);
  //  auto animationWalking = _core->createAnimation(gltfModelWalking);
  //  // set animation to model, so joints will be passed to shader
  //  modelWalking->setAnimation(animationWalking);
  //  {
  //    auto model = glm::translate(glm::mat4(1.f), glm::vec3(-2.f, 0.f, -3.f));
  //    model = glm::scale(model, glm::vec3(1.f, 1.f, 1.f));
  //    modelWalking->setModel(model);
  //  }
  //  _core->addDrawable(modelWalking);
  //}

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
  if (i > 150.f) _animationFish->setPlay(false);
  if (i > 200.f) _animationFish->setPlay(true);
  if (i > 250.f) _animationFish->setAnimation("bite");
  if (i > 350.f) _animationFish->setTime(0);
  if (i > 400.f) _animationFish->setTime(0.5f);
  if (i > 500.f) _animationFish->setAnimation(_animationFish->getAnimations()[0]);

  auto [FPSLimited, FPSReal] = _core->getFPS();
  auto [widthScreen, heightScreen] = _core->getState()->getSettings()->getResolution();
  _core->getGUI()->startWindow("Help", {20, 20}, {widthScreen / 10, 0});
  _core->getGUI()->drawText({"Limited FPS: " + std::to_string(FPSLimited)});
  _core->getGUI()->drawText({"Maximum FPS: " + std::to_string(FPSReal)});
  _core->getGUI()->drawText({"Press 'c' to turn cursor on/off"});
  _core->getGUI()->endWindow();
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