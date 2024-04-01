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
  _core->setCamera(_camera);

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
  auto ambientLight = lightManager->createAmbientLight();
  ambientLight->setColor({0.1f, 0.1f, 0.1f});

  // cube colored light
  _cubeColoredLightVertical = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  _cubeColoredLightVertical->getMesh()->setColor(
      std::vector{_cubeColoredLightVertical->getMesh()->getVertexData().size(), glm::vec3(1.f, 1.f, 1.f)},
      commandBufferTransfer);
  _core->addDrawable(_cubeColoredLightVertical);

  _cubeColoredLightHorizontal = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  _cubeColoredLightHorizontal->getMesh()->setColor(
      std::vector{_cubeColoredLightHorizontal->getMesh()->getVertexData().size(), glm::vec3(1.f, 1.f, 1.f)},
      commandBufferTransfer);
  _core->addDrawable(_cubeColoredLightHorizontal);

  auto cubeColoredLightDirectional = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
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
    if (material->getBaseColor().size() == 0)
      material->setBaseColor(std::vector{4, core->getResourceManager()->getTextureOne()});
    if (material->getNormal().size() == 0)
      material->setNormal(std::vector{4, core->getResourceManager()->getTextureZero()});
    if (material->getSpecular().size() == 0)
      material->setSpecular(std::vector{4, core->getResourceManager()->getTextureZero()});
  };

  auto fillMaterialPBR = [core = _core](std::shared_ptr<MaterialPBR> material) {
    if (material->getBaseColor().size() == 0)
      material->setBaseColor(std::vector{4, core->getResourceManager()->getTextureOne()});
    if (material->getNormal().size() == 0)
      material->setNormal(std::vector{4, core->getResourceManager()->getTextureZero()});
    if (material->getMetallic().size() == 0)
      material->setMetallic(std::vector{4, core->getResourceManager()->getTextureZero()});
    if (material->getRoughness().size() == 0)
      material->setRoughness(std::vector{4, core->getResourceManager()->getTextureZero()});
    if (material->getOccluded().size() == 0)
      material->setOccluded(std::vector{4, core->getResourceManager()->getTextureZero()});
    if (material->getEmissive().size() == 0)
      material->setEmissive(std::vector{4, core->getResourceManager()->getTextureZero()});
    material->setDiffuseIBL(core->getResourceManager()->getCubemapZero()->getTexture());
    material->setSpecularIBL(core->getResourceManager()->getCubemapZero()->getTexture(),
                             core->getResourceManager()->getTextureZero());
  };

  {
    auto tile0 = std::make_shared<Texture>(_core->getResourceManager()->loadImageGPU({"../assets/desert/albedo.png"}),
                                           settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                           mipMapLevels, commandBufferTransfer, state);
    auto tile1 = std::make_shared<Texture>(_core->getResourceManager()->loadImageGPU({"../assets/rock/albedo.png"}),
                                           settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                           mipMapLevels, commandBufferTransfer, state);
    auto tile2 = std::make_shared<Texture>(_core->getResourceManager()->loadImageGPU({"../assets/grass/albedo.png"}),
                                           settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                           mipMapLevels, commandBufferTransfer, state);
    auto tile3 = std::make_shared<Texture>(_core->getResourceManager()->loadImageGPU({"../assets/ground/albedo.png"}),
                                           settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                           mipMapLevels, commandBufferTransfer, state);

    _terrainColor = std::make_shared<Terrain>(
        _core->getResourceManager()->loadImageGPU({"../assets/heightmap.png"}), std::pair{12, 12},
        std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, commandBufferTransfer,
        lightManager, state);
    auto materialColor = std::make_shared<MaterialColor>(MaterialTarget::TERRAIN, commandBufferTransfer, state);
    materialColor->setBaseColor({tile0, tile1, tile2, tile3});
    _terrainColor->setMaterial(materialColor);
    {
      auto translateMatrix = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 0.f, 0.f));
      auto scaleMatrix = glm::scale(translateMatrix, glm::vec3(0.01f, 0.01f, 0.01f));
      _terrainColor->setModel(scaleMatrix);
    }

    _core->addDrawable(_terrainColor);
  }

  {
    auto tile0Color = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/desert/albedo.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile0Normal = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/desert/normal.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile1Color = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/rock/albedo.png"}), settings->getLoadTextureColorFormat(),
        VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer, state);
    auto tile1Normal = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/rock/normal.png"}), settings->getLoadTextureColorFormat(),
        VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer, state);
    auto tile2Color = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/grass/albedo.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile2Normal = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/grass/normal.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile3Color = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/ground/albedo.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile3Normal = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/ground/normal.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);

    _terrainPhong = std::make_shared<Terrain>(
        _core->getResourceManager()->loadImageGPU({"../assets/heightmap.png"}), std::pair{12, 12},
        std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, commandBufferTransfer,
        lightManager, state);
    auto materialPhong = std::make_shared<MaterialPhong>(MaterialTarget::TERRAIN, commandBufferTransfer, state);
    materialPhong->setBaseColor({tile0Color, tile1Color, tile2Color, tile3Color});
    materialPhong->setNormal({tile0Normal, tile1Normal, tile2Normal, tile3Normal});
    fillMaterialPhong(materialPhong);
    _terrainPhong->setMaterial(materialPhong);
    {
      auto translateMatrix = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, 0.f));
      auto scaleMatrix = glm::scale(translateMatrix, glm::vec3(0.01f, 0.01f, 0.01f));
      _terrainPhong->setModel(scaleMatrix);
    }

    _core->addDrawable(_terrainPhong);
  }

  {
    auto tile0Color = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/desert/albedo.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile0Normal = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/desert/normal.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile0Metallic = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/desert/metallic.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile0Roughness = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/desert/roughness.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile0AO = std::make_shared<Texture>(_core->getResourceManager()->loadImageGPU({"../assets/desert/ao.png"}),
                                             settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                             mipMapLevels, commandBufferTransfer, state);

    auto tile1Color = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/rock/albedo.png"}), settings->getLoadTextureColorFormat(),
        VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer, state);
    auto tile1Normal = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/rock/normal.png"}), settings->getLoadTextureColorFormat(),
        VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer, state);
    auto tile1Metallic = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/rock/metallic.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile1Roughness = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/rock/roughness.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile1AO = std::make_shared<Texture>(_core->getResourceManager()->loadImageGPU({"../assets/rock/ao.png"}),
                                             settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                             mipMapLevels, commandBufferTransfer, state);
    auto tile2Color = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/grass/albedo.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile2Normal = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/grass/normal.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile2Metallic = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/grass/metallic.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile2Roughness = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/grass/roughness.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile2AO = std::make_shared<Texture>(_core->getResourceManager()->loadImageGPU({"../assets/grass/ao.png"}),
                                             settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                             mipMapLevels, commandBufferTransfer, state);
    auto tile3Color = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/ground/albedo.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile3Normal = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/ground/normal.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile3Metallic = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/ground/metallic.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile3Roughness = std::make_shared<Texture>(
        _core->getResourceManager()->loadImageGPU({"../assets/ground/roughness.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile3AO = std::make_shared<Texture>(_core->getResourceManager()->loadImageGPU({"../assets/ground/ao.png"}),
                                             settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                             mipMapLevels, commandBufferTransfer, state);

    _terrainPBR = std::make_shared<Terrain>(
        _core->getResourceManager()->loadImageGPU({"../assets/heightmap.png"}), std::pair{12, 12},
        std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, commandBufferTransfer,
        lightManager, state);
    auto materialPBR = std::make_shared<MaterialPBR>(MaterialTarget::TERRAIN, commandBufferTransfer, state);
    materialPBR->setBaseColor({tile0Color, tile1Color, tile2Color, tile3Color});
    materialPBR->setNormal({tile0Normal, tile1Normal, tile2Normal, tile3Normal});
    materialPBR->setMetallic({tile0Metallic, tile1Metallic, tile2Metallic, tile3Metallic});
    materialPBR->setRoughness({tile0Roughness, tile1Roughness, tile2Roughness, tile3Roughness});
    materialPBR->setOccluded({tile0AO, tile1AO, tile2AO, tile3AO});
    fillMaterialPBR(materialPBR);
    _terrainPBR->setMaterial(materialPBR);
    {
      auto translateMatrix = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -3.f));
      auto scaleMatrix = glm::scale(translateMatrix, glm::vec3(0.01f, 0.01f, 0.01f));
      _terrainPBR->setModel(scaleMatrix);
    }

    _core->addDrawable(_terrainPBR);
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

  auto [FPSLimited, FPSReal] = _core->getFPS();
  auto [widthScreen, heightScreen] = _core->getState()->getSettings()->getResolution();
  _core->getGUI()->startWindow("Terrain", {20, 20}, {widthScreen / 10, heightScreen / 10});
  if (_core->getGUI()->startTree("Info")) {
    _core->getGUI()->drawText({"Limited FPS: " + std::to_string(FPSLimited)});
    _core->getGUI()->drawText({"Maximum FPS: " + std::to_string(FPSReal)});
    _core->getGUI()->drawText({"Press 'c' to turn cursor on/off"});
    _core->getGUI()->endTree();
  }
  if (_core->getGUI()->startTree("Toggles")) {
    if (_core->getGUI()->drawCheckbox({{"Patches", &_showPatches}})) {
      _terrainColor->patchEdge(_showPatches);
      _terrainPhong->patchEdge(_showPatches);
      _terrainPBR->patchEdge(_showPatches);
    }
    if (_core->getGUI()->drawCheckbox({{"LoD", &_showLoD}})) {
      _terrainColor->showLoD(_showLoD);
      _terrainPhong->showLoD(_showLoD);
      _terrainPBR->showLoD(_showLoD);
    }
    if (_core->getGUI()->drawCheckbox({{"Wireframe", &_showWireframe}})) {
      _terrainColor->setDrawType(DrawType::WIREFRAME);
      _terrainPhong->setDrawType(DrawType::WIREFRAME);
      _terrainPBR->setDrawType(DrawType::WIREFRAME);
      _showNormals = false;
    }
    if (_core->getGUI()->drawCheckbox({{"Normal", &_showNormals}})) {
      _terrainColor->setDrawType(DrawType::NORMAL);
      _terrainPhong->setDrawType(DrawType::NORMAL);
      _terrainPBR->setDrawType(DrawType::NORMAL);
      _showWireframe = false;
    }
    if (_showWireframe == false && _showNormals == false) {
      _terrainColor->setDrawType(DrawType::FILL);
      _terrainPhong->setDrawType(DrawType::FILL);
      _terrainPBR->setDrawType(DrawType::FILL);
    }
    _core->getGUI()->endTree();
  }
  _core->getGUI()->endWindow();
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