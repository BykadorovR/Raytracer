#include <iostream>
#include <chrono>
#include <future>
#include "Main.h"

void InputHandler::cursorNotify(GLFWwindow* window, float xPos, float yPos) {}

void InputHandler::mouseNotify(GLFWwindow* window, int button, int action, int mods) {}

void InputHandler::keyNotify(GLFWwindow* window, int key, int scancode, int action, int mods) {
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

void InputHandler::charNotify(GLFWwindow* window, unsigned int code) {}

void InputHandler::scrollNotify(GLFWwindow* window, double xOffset, double yOffset) {}

Main::Main() {
  int mipMapLevels = 4;
  auto settings = std::make_shared<Settings>();
  settings->setName("Sprite");
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
  auto commandBufferTransfer = _core->getCommandBufferTransfer();
  commandBufferTransfer->beginCommands();
  auto state = _core->getState();
  _camera = std::make_shared<CameraFly>(settings);
  _camera->setProjectionParameters(60.f, 0.1f, 100.f);
  _core->getState()->getInput()->subscribe(std::dynamic_pointer_cast<InputSubscriber>(_camera));
  _inputHandler = std::make_shared<InputHandler>();
  _core->getState()->getInput()->subscribe(std::dynamic_pointer_cast<InputSubscriber>(_inputHandler));

  auto lightManager = _core->getLightManager();
  _pointLightVertical = lightManager->createPointLight(settings->getDepthResolution());
  _pointLightVertical->setColor(glm::vec3(1.f, 1.f, 1.f));
  _pointLightHorizontal = lightManager->createPointLight(settings->getDepthResolution());
  _pointLightHorizontal->setColor(glm::vec3(1.f, 1.f, 1.f));
  _directionalLight = lightManager->createDirectionalLight(settings->getDepthResolution());
  _directionalLight->setColor(glm::vec3(1.f, 1.f, 1.f));
  _directionalLight->setPosition(glm::vec3(0.f, 20.f, 0.f));
  // TODO: rename setCenter to lookAt
  //  looking to (0.f, 0.f, 0.f) with up vector (0.f, 0.f, -1.f)
  _directionalLight->setCenter({0.f, 0.f, 0.f});
  _directionalLight->setUp({0.f, 0.f, -1.f});

  // cube colored light
  _cubeColoredLightVertical = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  _cubeColoredLightVertical->setCamera(_camera);
  _cubeColoredLightVertical->getMesh()->setColor(
      std::vector{_cubeColoredLightVertical->getMesh()->getVertexData().size(), glm::vec3(1.f, 1.f, 1.f)},
      commandBufferTransfer);
  _core->addDrawable(_cubeColoredLightVertical);

  _cubeColoredLightHorizontal = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  _cubeColoredLightHorizontal->setCamera(_camera);
  _cubeColoredLightHorizontal->getMesh()->setColor(
      std::vector{_cubeColoredLightHorizontal->getMesh()->getVertexData().size(), glm::vec3(1.f, 1.f, 1.f)},
      commandBufferTransfer);
  _core->addDrawable(_cubeColoredLightHorizontal);

  auto cubeColoredLightDirectional = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  cubeColoredLightDirectional->setCamera(_camera);
  cubeColoredLightDirectional->getMesh()->setColor(
      std::vector{cubeColoredLightDirectional->getMesh()->getVertexData().size(), glm::vec3(1.f, 1.f, 1.f)},
      commandBufferTransfer);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 20.f, 0.f));
    model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f));
    cubeColoredLightDirectional->setModel(model);
  }
  _core->addDrawable(cubeColoredLightDirectional);

  auto fillMaterialPhong = [core = _core](std::shared_ptr<MaterialPhong> material) {
    if (material->getBaseColor() == nullptr) material->setBaseColor(core->getResourceManager()->getTextureOne());
    if (material->getNormal() == nullptr) material->setNormal(core->getResourceManager()->getTextureZero());
    if (material->getSpecular() == nullptr) material->setSpecular(core->getResourceManager()->getTextureZero());
  };

  auto fillMaterialPBR = [core = _core](std::shared_ptr<MaterialPBR> material) {
    if (material->getBaseColor() == nullptr) material->setBaseColor(core->getResourceManager()->getTextureOne());
    if (material->getNormal() == nullptr) material->setNormal(core->getResourceManager()->getTextureZero());
    if (material->getMetallic() == nullptr) material->setMetallic(core->getResourceManager()->getTextureZero());
    if (material->getRoughness() == nullptr) material->setRoughness(core->getResourceManager()->getTextureZero());
    if (material->getOccluded() == nullptr) material->setOccluded(core->getResourceManager()->getTextureZero());
    if (material->getEmissive() == nullptr) material->setEmissive(core->getResourceManager()->getTextureZero());
    material->setDiffuseIBL(core->getResourceManager()->getCubemapZero()->getTexture());
    material->setSpecularIBL(core->getResourceManager()->getCubemapZero()->getTexture(),
                             core->getResourceManager()->getTextureZero());
  };

  auto spriteManager = std::make_shared<SpriteManager>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, lightManager,
      commandBufferTransfer, _core->getResourceManager(), state);
  spriteManager->setCamera(_camera);
  // draw textured Sprite without lighting
  {
    auto textureTree = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"tree.png"}), settings->getLoadTextureAuxilaryFormat(),
        VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer, state);
    auto spriteTree = spriteManager->createSprite();
    spriteManager->registerSprite(spriteTree);
    auto materialColor = std::make_shared<MaterialColor>(commandBufferTransfer, state);
    materialColor->setBaseColor(textureTree);
    spriteTree->setMaterial(materialColor);
  }

  commandBufferTransfer->endCommands();
  // TODO: remove vkQueueWaitIdle, add fence or semaphore
  // TODO: move this function to core
  {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBufferTransfer->getCommandBuffer()[0];
    auto queue = state->getDevice()->getQueue(QueueType::GRAPHIC);
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
  }

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
  _core->getGUI()->drawText("Help", {20, 20}, {"Limited FPS: " + std::to_string(FPSLimited)});
  _core->getGUI()->drawText("Help", {20, 20}, {"Maximum FPS: " + std::to_string(FPSReal)});
  _core->getGUI()->drawText("Help", {20, 20}, {"Press 'c' to turn cursor on/off"});
}

void Main::reset(int width, int height) { _camera->setAspect((float)width / (float)height); }

void Main::start() { _core->draw(); }

int main() {
#ifdef WIN32
  SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
#endif
  try {
    auto main = std::make_shared<Main>();
    main->start();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}