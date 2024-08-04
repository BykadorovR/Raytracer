#include <iostream>
#include <chrono>
#include <future>
#include "Main.h"
#include <glm/gtx/matrix_decompose.hpp>

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

  _cubePlayer = _core->createShape3D(ShapeType::CUBE);
  _cubePlayer->getMesh()->setColor(
      std::vector{_cubePlayer->getMesh()->getVertexData().size(), glm::vec3(0.f, 0.f, 1.f)},
      _core->getCommandBufferApplication());
  auto callbackPosition = [player = _cubePlayer, heightmap = heightmapCPU](glm::vec3 shift) {
    glm::vec3 position = getPosition(player);
    position += shift;
    auto height = getHeight(heightmap, position);
    position.y = height;
    auto model = glm::translate(glm::mat4(1.f), position);
    player->setModel(model);
  };
  _inputHandler->setMoveCallback(callbackPosition);

  {
    glm::vec3 position(0.f);
    auto height = getHeight(heightmapCPU, position);
    position.y = height;
    auto translateMatrix = glm::translate(glm::mat4(1.f), position);
    _cubePlayer->setModel(translateMatrix);
  }
  _core->addDrawable(_cubePlayer);

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