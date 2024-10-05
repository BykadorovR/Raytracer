#include <iostream>
#include <chrono>
#include <future>
#include "Main.h"

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

void Main::_createTerrainPhong() {
  _core->removeDrawable(_terrain);
  _terrain = _core->createTerrain(_core->loadImageCPU("../assets/heightmap.png"), {_patchX, _patchY});
  _terrain->setMaterial(_materialPhong);
  {
    auto translateMatrix = glm::translate(glm::mat4(1.f), _terrainPosition);
    auto scaleMatrix = glm::scale(translateMatrix, _terrainScale);
    _terrain->setModel(scaleMatrix);
  }

  _terrain->setTessellationLevel(_minTessellationLevel, _maxTessellationLevel);
  _terrain->setDisplayDistance(_minDistance, _maxDistance);
  _terrain->setColorHeightLevels(_heightLevels);
  _terrain->setHeight(_heightScale, _heightShift);

  _core->addDrawable(_terrain);
}

void Main::_createTerrainPBR() {
  _core->removeDrawable(_terrain);
  _terrain = _core->createTerrain(_core->loadImageCPU("../assets/heightmap.png"), {_patchX, _patchY});
  _terrain->setMaterial(_materialPBR);
  {
    auto translateMatrix = glm::translate(glm::mat4(1.f), _terrainPosition);
    auto scaleMatrix = glm::scale(translateMatrix, _terrainScale);
    _terrain->setModel(scaleMatrix);
  }

  _terrain->setTessellationLevel(_minTessellationLevel, _maxTessellationLevel);
  _terrain->setDisplayDistance(_minDistance, _maxDistance);
  _terrain->setColorHeightLevels(_heightLevels);
  _terrain->setHeight(_heightScale, _heightShift);

  _core->addDrawable(_terrain);
}

void Main::_createTerrainColor() {
  _core->removeDrawable(_terrain);
  _terrain = _core->createTerrain(_core->loadImageCPU("../assets/heightmap.png"), {_patchX, _patchY});
  _terrain->setMaterial(_materialColor);
  {
    auto translateMatrix = glm::translate(glm::mat4(1.f), _terrainPosition);
    auto scaleMatrix = glm::scale(translateMatrix, _terrainScale);
    _terrain->setModel(scaleMatrix);
  }

  _terrain->setTessellationLevel(_minTessellationLevel, _maxTessellationLevel);
  _terrain->setDisplayDistance(_minDistance, _maxDistance);
  _terrain->setColorHeightLevels(_heightLevels);
  _terrain->setHeight(_heightScale, _heightShift);

  _core->addDrawable(_terrain);
}

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
  _core->initialize();
  _core->startRecording();
  auto state = _core->getState();
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
    auto tile0 = _core->createTexture("../assets/desert/albedo.png", settings->getLoadTextureColorFormat(),
                                      mipMapLevels);
    auto tile1 = _core->createTexture("../assets/rock/albedo.png", settings->getLoadTextureColorFormat(), mipMapLevels);
    auto tile2 = _core->createTexture("../assets/grass/albedo.png", settings->getLoadTextureColorFormat(),
                                      mipMapLevels);
    auto tile3 = _core->createTexture("../assets/ground/albedo.png", settings->getLoadTextureColorFormat(),
                                      mipMapLevels);
    _materialColor = _core->createMaterialColor(MaterialTarget::TERRAIN);
    _materialColor->setBaseColor({tile0, tile1, tile2, tile3});
  }

  {
    auto tile0Color = _core->createTexture("../assets/desert/albedo.png", settings->getLoadTextureColorFormat(),
                                           mipMapLevels);
    auto tile0Normal = _core->createTexture("../assets/desert/normal.png", settings->getLoadTextureAuxilaryFormat(),
                                            mipMapLevels);
    auto tile1Color = _core->createTexture("../assets/rock/albedo.png", settings->getLoadTextureColorFormat(),
                                           mipMapLevels);
    auto tile1Normal = _core->createTexture("../assets/rock/normal.png", settings->getLoadTextureAuxilaryFormat(),
                                            mipMapLevels);
    auto tile2Color = _core->createTexture("../assets/grass/albedo.png", settings->getLoadTextureColorFormat(),
                                           mipMapLevels);
    auto tile2Normal = _core->createTexture("../assets/grass/normal.png", settings->getLoadTextureAuxilaryFormat(),
                                            mipMapLevels);
    auto tile3Color = _core->createTexture("../assets/ground/albedo.png", settings->getLoadTextureColorFormat(),
                                           mipMapLevels);
    auto tile3Normal = _core->createTexture("../assets/ground/normal.png", settings->getLoadTextureAuxilaryFormat(),
                                            mipMapLevels);

    _materialPhong = _core->createMaterialPhong(MaterialTarget::TERRAIN);
    _materialPhong->setBaseColor({tile0Color, tile1Color, tile2Color, tile3Color});
    _materialPhong->setNormal({tile0Normal, tile1Normal, tile2Normal, tile3Normal});
    fillMaterialPhong(_materialPhong);
  }

  {
    auto tile0Color = _core->createTexture("../assets/desert/albedo.png", settings->getLoadTextureColorFormat(),
                                           mipMapLevels);
    auto tile0Normal = _core->createTexture("../assets/desert/normal.png", settings->getLoadTextureAuxilaryFormat(),
                                            mipMapLevels);
    auto tile0Metallic = _core->createTexture("../assets/desert/metallic.png", settings->getLoadTextureAuxilaryFormat(),
                                              mipMapLevels);
    auto tile0Roughness = _core->createTexture("../assets/desert/roughness.png",
                                               settings->getLoadTextureAuxilaryFormat(), mipMapLevels);
    auto tile0AO = _core->createTexture("../assets/desert/ao.png", settings->getLoadTextureAuxilaryFormat(),
                                        mipMapLevels);

    auto tile1Color = _core->createTexture("../assets/rock/albedo.png", settings->getLoadTextureColorFormat(),
                                           mipMapLevels);
    auto tile1Normal = _core->createTexture("../assets/rock/normal.png", settings->getLoadTextureAuxilaryFormat(),
                                            mipMapLevels);
    auto tile1Metallic = _core->createTexture("../assets/rock/metallic.png", settings->getLoadTextureAuxilaryFormat(),
                                              mipMapLevels);
    auto tile1Roughness = _core->createTexture("../assets/rock/roughness.png", settings->getLoadTextureAuxilaryFormat(),
                                               mipMapLevels);
    auto tile1AO = _core->createTexture("../assets/rock/ao.png", settings->getLoadTextureAuxilaryFormat(),
                                        mipMapLevels);

    auto tile2Color = _core->createTexture("../assets/grass/albedo.png", settings->getLoadTextureColorFormat(),
                                           mipMapLevels);
    auto tile2Normal = _core->createTexture("../assets/grass/normal.png", settings->getLoadTextureAuxilaryFormat(),
                                            mipMapLevels);
    auto tile2Metallic = _core->createTexture("../assets/grass/metallic.png", settings->getLoadTextureAuxilaryFormat(),
                                              mipMapLevels);
    auto tile2Roughness = _core->createTexture("../assets/grass/roughness.png",
                                               settings->getLoadTextureAuxilaryFormat(), mipMapLevels);
    auto tile2AO = _core->createTexture("../assets/grass/ao.png", settings->getLoadTextureAuxilaryFormat(),
                                        mipMapLevels);

    auto tile3Color = _core->createTexture("../assets/ground/albedo.png", settings->getLoadTextureColorFormat(),
                                           mipMapLevels);
    auto tile3Normal = _core->createTexture("../assets/ground/normal.png", settings->getLoadTextureAuxilaryFormat(),
                                            mipMapLevels);
    auto tile3Metallic = _core->createTexture("../assets/ground/metallic.png", settings->getLoadTextureAuxilaryFormat(),
                                              mipMapLevels);
    auto tile3Roughness = _core->createTexture("../assets/ground/roughness.png",
                                               settings->getLoadTextureAuxilaryFormat(), mipMapLevels);
    auto tile3AO = _core->createTexture("../assets/ground/ao.png", settings->getLoadTextureAuxilaryFormat(),
                                        mipMapLevels);

    _materialPBR = _core->createMaterialPBR(MaterialTarget::TERRAIN);
    _materialPBR->setBaseColor({tile0Color, tile1Color, tile2Color, tile3Color});
    _materialPBR->setNormal({tile0Normal, tile1Normal, tile2Normal, tile3Normal});
    _materialPBR->setMetallic({tile0Metallic, tile1Metallic, tile2Metallic, tile3Metallic});
    _materialPBR->setRoughness({tile0Roughness, tile1Roughness, tile2Roughness, tile3Roughness});
    _materialPBR->setOccluded({tile0AO, tile1AO, tile2AO, tile3AO});
    fillMaterialPBR(_materialPBR);
  }

  _createTerrainColor();

  auto terrainCPU = _core->loadImageCPU("../assets/heightmap.png");
  auto [terrainWidth, terrainHeight] = terrainCPU->getResolution();
  _terrainPositionDebug = glm::vec3(_terrainPositionDebug.x + 256 * _terrainScale.x, _terrainPositionDebug.y,
                                    _terrainPositionDebug.z);

  _physicsManager = std::make_shared<PhysicsManager>();
  _terrainPhysics = std::make_shared<TerrainPhysics>(_core->loadImageCPU("../assets/heightmap.png"),
                                                     _terrainPositionDebug, _terrainScale, std::tuple{64, 16},
                                                     _physicsManager);
  _terrainDebug = std::make_shared<TerrainDebug>(_core->loadImageGPU(_core->loadImageCPU("../assets/heightmap.png")),
                                                 std::pair{_patchX, _patchY}, _core->getCommandBufferApplication(),
                                                 _core->getGUI(), _core->getGameState(), _core->getState());
  _core->getState()->getInput()->subscribe(std::dynamic_pointer_cast<InputSubscriber>(_terrainDebug));
  _terrainDebug->setTerrainPhysics(_terrainPhysics);
  _terrainDebug->setMaterial(_materialColor);

  _terrainDebug->setTessellationLevel(_minTessellationLevel, _maxTessellationLevel);
  _terrainDebug->setDisplayDistance(_minDistance, _maxDistance);
  _terrainDebug->setColorHeightLevels(_heightLevels);
  _terrainDebug->setHeight(_heightScale, _heightShift);
  _terrainDebug->patchEdge(_showPatches);
  _terrainDebug->showLoD(_showLoD);
  if (_showWireframe) {
    _terrainDebug->setDrawType(DrawType::WIREFRAME);
  }
  if (_showNormals) {
    _terrainDebug->setDrawType(DrawType::NORMAL);
  }
  if (_showWireframe == false && _showNormals == false) {
    _terrainDebug->setDrawType(DrawType::FILL);
  }
  {
    auto translateMatrix = glm::translate(glm::mat4(1.f), _terrainPositionDebug);
    auto scaleMatrix = glm::scale(translateMatrix, _terrainScale);
    _terrainDebug->setModel(scaleMatrix);
  }
  _core->addDrawable(_terrainDebug);

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
  if (_core->getGUI()->startTree("Toggles")) {
    std::map<std::string, int*> terrainType;
    terrainType["##Type"] = &_typeIndex;
    if (_core->getGUI()->drawListBox({"Color", "Phong", "PBR"}, terrainType, 3)) {
      _core->startRecording();
      switch (_typeIndex) {
        case 0:
          _createTerrainColor();
          break;
        case 1:
          _createTerrainPhong();
          break;
        case 2:
          _createTerrainPBR();
          break;
      }
      _core->endRecording();
    }

    std::map<std::string, int*> patchesNumber{{"Patch x", &_patchX}, {"Patch y", &_patchY}};
    if (_core->getGUI()->drawInputInt(patchesNumber)) {
      _core->startRecording();
      switch (_typeIndex) {
        case 0:
          _createTerrainColor();
          break;
        case 1:
          _createTerrainPhong();
          break;
        case 2:
          _createTerrainPBR();
          break;
      }
      _core->endRecording();
    }

    std::map<std::string, int*> tesselationLevels{{"Tesselation min", &_minTessellationLevel},
                                                  {"Tesselation max", &_maxTessellationLevel}};
    if (_core->getGUI()->drawInputInt(tesselationLevels)) {
      _terrain->setTessellationLevel(_minTessellationLevel, _maxTessellationLevel);
    }

    _core->getGUI()->endTree();
  }
  _core->getGUI()->endWindow();

  _core->getGUI()->startWindow("Editor", {widthScreen - widthScreen / 7, 20}, {widthScreen / 10, heightScreen / 10});
  _terrainDebug->drawDebug();
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