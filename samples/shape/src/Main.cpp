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
  settings->setName("Shape");
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

  // cube colored
  auto cubeColored = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  cubeColored->setCamera(_camera);
  cubeColored->getMesh()->setColor(
      std::vector{cubeColored->getMesh()->getVertexData().size(), glm::vec3(1.f, 0.f, 0.f)}, commandBufferTransfer);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 3.f, -3.f));
    cubeColored->setModel(model);
  }
  _core->addDrawable(cubeColored);
  // cube textured
  auto cubemap = std::make_shared<Cubemap>(
      _core->getResourceManager()->loadImage(std::vector<std::string>{"../assets/right.jpg", "../assets/left.jpg",
                                                                      "../assets/top.jpg", "../assets/bottom.jpg",
                                                                      "../assets/front.jpg", "../assets/back.jpg"}),
      settings->getLoadTextureColorFormat(), mipMapLevels, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      commandBufferTransfer, state);
  auto materialCubeTextured = std::make_shared<MaterialColor>(commandBufferTransfer, state);
  materialCubeTextured->setBaseColor(cubemap->getTexture());
  _cubeTextured = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  _cubeTextured->setCamera(_camera);
  _cubeTextured->setMaterial(materialCubeTextured);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -3.f));
    _cubeTextured->setModel(model);
  }
  _core->addDrawable(_cubeTextured);

  //  cube Phong
  auto cubemapColorPhong = std::make_shared<Cubemap>(
      _core->getResourceManager()->loadImage(
          std::vector<std::string>{"../assets/brickwall.jpg", "../assets/brickwall.jpg", "../assets/brickwall.jpg",
                                   "../assets/brickwall.jpg", "../assets/brickwall.jpg", "../assets/brickwall.jpg"}),
      settings->getLoadTextureColorFormat(), mipMapLevels, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      commandBufferTransfer, state);
  auto cubemapNormalPhong = std::make_shared<Cubemap>(
      _core->getResourceManager()->loadImage(std::vector<std::string>{
          "../assets/brickwall_normal.jpg", "../assets/brickwall_normal.jpg", "../assets/brickwall_normal.jpg",
          "../assets/brickwall_normal.jpg", "../assets/brickwall_normal.jpg", "../assets/brickwall_normal.jpg"}),
      settings->getLoadTextureColorFormat(), mipMapLevels, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      commandBufferTransfer, state);
  auto materialCubePhong = std::make_shared<MaterialPhong>(commandBufferTransfer, state);
  materialCubePhong->setBaseColor(cubemapColorPhong->getTexture());
  materialCubePhong->setNormal(cubemapNormalPhong->getTexture());
  materialCubePhong->setSpecular(_core->getResourceManager()->getCubemapZero()->getTexture());

  auto cubeTexturedPhong = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  cubeTexturedPhong->setCamera(_camera);
  cubeTexturedPhong->setMaterial(materialCubePhong);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -3.f, -3.f));
    cubeTexturedPhong->setModel(model);
  }
  _core->addDrawable(cubeTexturedPhong);

  // cube colored wireframe
  auto cubeColoredWireframe = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  cubeColoredWireframe->setCamera(_camera);
  cubeColoredWireframe->setDrawType(DrawType::WIREFRAME);
  cubeColoredWireframe->getMesh()->setColor(
      std::vector{cubeColoredWireframe->getMesh()->getVertexData().size(), glm::vec3(1.f, 0.f, 0.f)},
      commandBufferTransfer);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 3.f, -3.f));
    cubeColoredWireframe->setModel(model);
  }
  _core->addDrawable(cubeColoredWireframe);

  // cube texture wireframe
  _cubeTexturedWireframe = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  _cubeTexturedWireframe->setCamera(_camera);
  _cubeTexturedWireframe->setDrawType(DrawType::WIREFRAME);
  _cubeTexturedWireframe->setMaterial(materialCubeTextured);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 0.f, -3.f));
    _cubeTexturedWireframe->setModel(model);
  }
  _core->addDrawable(_cubeTexturedWireframe);

  // cube Phong wireframe
  auto cubeTexturedWireframePhong = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  cubeTexturedWireframePhong->setCamera(_camera);
  cubeTexturedWireframePhong->setDrawType(DrawType::WIREFRAME);
  cubeTexturedWireframePhong->setMaterial(materialCubePhong);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, -3.f, -3.f));
    cubeTexturedWireframePhong->setModel(model);
  }
  _core->addDrawable(cubeTexturedWireframePhong);

  // cube Phong mesh normal
  auto cubeTexturedPhongNormalMesh = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  cubeTexturedPhongNormalMesh->setCamera(_camera);
  cubeTexturedPhongNormalMesh->setDrawType(DrawType::NORMAL);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(-3.f, -3.f, -3.f));
    cubeTexturedPhongNormalMesh->setModel(model);
  }
  _core->addDrawable(cubeTexturedPhongNormalMesh);

  // cube Color fragment normal
  auto cubemapNormal = std::make_shared<Cubemap>(
      _core->getResourceManager()->loadImage(std::vector<std::string>{
          "../assets/brickwall_normal.jpg", "../assets/brickwall_normal.jpg", "../assets/brickwall_normal.jpg",
          "../assets/brickwall_normal.jpg", "../assets/brickwall_normal.jpg", "../assets/brickwall_normal.jpg"}),
      settings->getLoadTextureColorFormat(), mipMapLevels, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      commandBufferTransfer, state);
  auto materialCubeTexturedNormalFragment = std::make_shared<MaterialColor>(commandBufferTransfer, state);
  materialCubeTexturedNormalFragment->setBaseColor(cubemapNormal->getTexture());
  auto cubeTexturedNormalFragment = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  cubeTexturedNormalFragment->setCamera(_camera);
  cubeTexturedNormalFragment->setMaterial(materialCubeTexturedNormalFragment);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(-3.f, -3.f, -3.f));
    cubeTexturedNormalFragment->setModel(model);
  }
  _core->addDrawable(cubeTexturedNormalFragment);

  // cube Phong: specular without normal
  auto cubemapColorPhongContainer = std::make_shared<Cubemap>(
      _core->getResourceManager()->loadImage(
          std::vector<std::string>{"../assets/container.png", "../assets/container.png", "../assets/container.png",
                                   "../assets/container.png", "../assets/container.png", "../assets/container.png"}),
      settings->getLoadTextureColorFormat(), mipMapLevels, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      commandBufferTransfer, state);
  auto cubemapSpecularPhong = std::make_shared<Cubemap>(
      _core->getResourceManager()->loadImage(std::vector<std::string>{
          "../assets/container_specular.png", "../assets/container_specular.png", "../assets/container_specular.png",
          "../assets/container_specular.png", "../assets/container_specular.png", "../assets/container_specular.png"}),
      settings->getLoadTextureColorFormat(), mipMapLevels, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      commandBufferTransfer, state);
  auto materialCubePhongSpecular = std::make_shared<MaterialPhong>(commandBufferTransfer, state);
  materialCubePhongSpecular->setBaseColor(cubemapColorPhongContainer->getTexture());
  materialCubePhongSpecular->setNormal(_core->getResourceManager()->getCubemapZero()->getTexture());
  materialCubePhongSpecular->setSpecular(cubemapSpecularPhong->getTexture());
  materialCubePhongSpecular->setCoefficients(glm::vec3(0.f), glm::vec3(0.2f), glm::vec3(1.f), 32);

  auto cubeTexturedPhongSpecular = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  cubeTexturedPhongSpecular->setCamera(_camera);
  cubeTexturedPhongSpecular->setMaterial(materialCubePhongSpecular);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -6.f, -3.f));
    cubeTexturedPhongSpecular->setModel(model);
  }
  _core->addDrawable(cubeTexturedPhongSpecular);

  // cube PBR
  // TODO: fix mipmaping
  auto cubemapColorPBR = std::make_shared<Cubemap>(
      _core->getResourceManager()->loadImage(
          std::vector<std::string>{"../assets/rustediron2_basecolor.png", "../assets/rustediron2_basecolor.png",
                                   "../assets/rustediron2_basecolor.png", "../assets/rustediron2_basecolor.png",
                                   "../assets/rustediron2_basecolor.png", "../assets/rustediron2_basecolor.png"}),
      settings->getLoadTextureColorFormat(), mipMapLevels, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      commandBufferTransfer, state);
  auto cubemapNormalPBR = std::make_shared<Cubemap>(
      _core->getResourceManager()->loadImage(std::vector<std::string>{
          "../assets/rustediron2_normal.png", "../assets/rustediron2_normal.png", "../assets/rustediron2_normal.png",
          "../assets/rustediron2_normal.png", "../assets/rustediron2_normal.png", "../assets/rustediron2_normal.png"}),
      settings->getLoadTextureColorFormat(), mipMapLevels, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      commandBufferTransfer, state);
  auto cubemapMetallicPBR = std::make_shared<Cubemap>(
      _core->getResourceManager()->loadImage(
          std::vector<std::string>{"../assets/rustediron2_metallic.png", "../assets/rustediron2_metallic.png",
                                   "../assets/rustediron2_metallic.png", "../assets/rustediron2_metallic.png",
                                   "../assets/rustediron2_metallic.png", "../assets/rustediron2_metallic.png"}),
      settings->getLoadTextureColorFormat(), mipMapLevels, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      commandBufferTransfer, state);
  auto cubemapRoughnessPBR = std::make_shared<Cubemap>(
      _core->getResourceManager()->loadImage(
          std::vector<std::string>{"../assets/rustediron2_roughness.png", "../assets/rustediron2_roughness.png",
                                   "../assets/rustediron2_roughness.png", "../assets/rustediron2_roughness.png",
                                   "../assets/rustediron2_roughness.png", "../assets/rustediron2_roughness.png"}),
      settings->getLoadTextureColorFormat(), mipMapLevels, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      commandBufferTransfer, state);
  auto materialCubePBR = std::make_shared<MaterialPBR>(commandBufferTransfer, state);
  // material can't have default state because it can be either cubemap or texture2D
  materialCubePBR->setBaseColor(cubemapColorPBR->getTexture());
  materialCubePBR->setNormal(cubemapNormalPBR->getTexture());
  materialCubePBR->setMetallic(cubemapMetallicPBR->getTexture());
  materialCubePBR->setRoughness(cubemapRoughnessPBR->getTexture());
  materialCubePBR->setEmissive(_core->getResourceManager()->getCubemapZero()->getTexture());
  materialCubePBR->setOccluded(_core->getResourceManager()->getCubemapZero()->getTexture());
  materialCubePBR->setDiffuseIBL(_core->getResourceManager()->getCubemapZero()->getTexture());
  materialCubePBR->setSpecularIBL(_core->getResourceManager()->getCubemapZero()->getTexture(),
                                  _core->getResourceManager()->getTextureZero());

  auto cubeTexturedPBR = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  cubeTexturedPBR->setCamera(_camera);
  cubeTexturedPBR->setMaterial(materialCubePBR);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 6.f, -3.f));
    cubeTexturedPBR->setModel(model);
  }
  _core->addDrawable(cubeTexturedPBR);

  // cube PBR wireframe
  auto cubeWireframePBR = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  cubeWireframePBR->setCamera(_camera);
  cubeWireframePBR->setMaterial(materialCubePBR);
  cubeWireframePBR->setDrawType(DrawType::WIREFRAME);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 6.f, -3.f));
    cubeWireframePBR->setModel(model);
  }
  _core->addDrawable(cubeWireframePBR);

  // sphere colored
  auto sphereColored = std::make_shared<Shape3D>(
      ShapeType::SPHERE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  sphereColored->setCamera(_camera);
  sphereColored->getMesh()->setColor(
      std::vector{sphereColored->getMesh()->getVertexData().size(), glm::vec3(0.f, 1.f, 0.f)}, commandBufferTransfer);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 3.f, 3.f));
    sphereColored->setModel(model);
  }
  _core->addDrawable(sphereColored);

  // sphere textured
  auto sphereTexture = std::make_shared<Texture>(
      _core->getResourceManager()->loadImage({"../assets/right.jpg"}), settings->getLoadTextureAuxilaryFormat(),
      VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer, state);
  auto materialSphereTextured = std::make_shared<MaterialColor>(commandBufferTransfer, state);
  materialSphereTextured->setBaseColor(sphereTexture);
  auto sphereTextured = std::make_shared<Shape3D>(
      ShapeType::SPHERE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  sphereTextured->setCamera(_camera);
  sphereTextured->setMaterial(materialSphereTextured);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, 3.f));
    sphereTextured->setModel(model);
  }
  _core->addDrawable(sphereTextured);

  // sphere Phong
  auto sphereTexturePhong = std::make_shared<Texture>(
      _core->getResourceManager()->loadImage({"../assets/brickwall.jpg"}), settings->getLoadTextureAuxilaryFormat(),
      VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer, state);
  auto sphereNormalPhong = std::make_shared<Texture>(
      _core->getResourceManager()->loadImage({"../assets/brickwall_normal.jpg"}),
      settings->getLoadTextureAuxilaryFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
      state);
  auto materialSpherePhong = std::make_shared<MaterialPhong>(commandBufferTransfer, state);
  materialSpherePhong->setBaseColor(sphereTexturePhong);
  materialSpherePhong->setNormal(sphereNormalPhong);
  materialSpherePhong->setSpecular(_core->getResourceManager()->getTextureZero());

  auto sphereTexturedPhong = std::make_shared<Shape3D>(
      ShapeType::SPHERE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  sphereTexturedPhong->setCamera(_camera);
  sphereTexturedPhong->setMaterial(materialSpherePhong);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -3.f, 3.f));
    sphereTexturedPhong->setModel(model);
  }
  _core->addDrawable(sphereTexturedPhong);

  // sphere colored wireframe
  auto sphereColoredWireframe = std::make_shared<Shape3D>(
      ShapeType::SPHERE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  sphereColoredWireframe->setCamera(_camera);
  sphereColoredWireframe->setDrawType(DrawType::WIREFRAME);
  sphereColoredWireframe->getMesh()->setColor(
      std::vector{sphereColoredWireframe->getMesh()->getVertexData().size(), glm::vec3(0.f, 1.f, 0.f)},
      commandBufferTransfer);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 3.f, 3.f));
    sphereColoredWireframe->setModel(model);
  }
  _core->addDrawable(sphereColoredWireframe);

  // sphere texture wireframe
  auto sphereTexturedWireframe = std::make_shared<Shape3D>(
      ShapeType::SPHERE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  sphereTexturedWireframe->setCamera(_camera);
  sphereTexturedWireframe->setDrawType(DrawType::WIREFRAME);
  sphereTexturedWireframe->setMaterial(materialSphereTextured);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 0.f, 3.f));
    sphereTexturedWireframe->setModel(model);
  }
  _core->addDrawable(sphereTexturedWireframe);

  // sphere Phong wireframe
  auto sphereTexturedWireframePhong = std::make_shared<Shape3D>(
      ShapeType::SPHERE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  sphereTexturedWireframePhong->setCamera(_camera);
  sphereTexturedWireframePhong->setDrawType(DrawType::WIREFRAME);
  sphereTexturedWireframePhong->setMaterial(materialSpherePhong);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, -3.f, 3.f));
    sphereTexturedWireframePhong->setModel(model);
  }
  _core->addDrawable(sphereTexturedWireframePhong);

  // sphere Phong mesh normal
  auto sphereTexturedPhongNormalMesh = std::make_shared<Shape3D>(
      ShapeType::SPHERE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  sphereTexturedPhongNormalMesh->setCamera(_camera);
  sphereTexturedPhongNormalMesh->setDrawType(DrawType::NORMAL);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(-3.f, -3.f, 3.f));
    sphereTexturedPhongNormalMesh->setModel(model);
  }
  _core->addDrawable(sphereTexturedPhongNormalMesh);

  // sphere Color fragment normal
  auto materialSphereTexturedNormalFragment = std::make_shared<MaterialColor>(commandBufferTransfer, state);
  materialSphereTexturedNormalFragment->setBaseColor(sphereNormalPhong);
  auto sphereTexturedNormalFragment = std::make_shared<Shape3D>(
      ShapeType::SPHERE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  sphereTexturedNormalFragment->setCamera(_camera);
  sphereTexturedNormalFragment->setMaterial(materialSphereTexturedNormalFragment);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(-3.f, -3.f, 3.f));
    sphereTexturedNormalFragment->setModel(model);
  }
  _core->addDrawable(sphereTexturedNormalFragment);

  // cube Phong: specular without normal
  auto sphereColorPhongContainer = std::make_shared<Texture>(
      _core->getResourceManager()->loadImage({"../assets/container.png"}), settings->getLoadTextureAuxilaryFormat(),
      VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer, state);
  auto sphereSpecularPhongContainer = std::make_shared<Texture>(
      _core->getResourceManager()->loadImage({"../assets/container_specular.png"}),
      settings->getLoadTextureAuxilaryFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
      state);
  auto materialSpherePhongSpecular = std::make_shared<MaterialPhong>(commandBufferTransfer, state);
  materialSpherePhongSpecular->setBaseColor(sphereColorPhongContainer);
  materialSpherePhongSpecular->setNormal(_core->getResourceManager()->getTextureZero());
  materialSpherePhongSpecular->setSpecular(sphereSpecularPhongContainer);
  materialSpherePhongSpecular->setCoefficients(glm::vec3(0.f), glm::vec3(0.2f), glm::vec3(1.f), 32);

  auto sphereTexturedPhongSpecular = std::make_shared<Shape3D>(
      ShapeType::SPHERE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  sphereTexturedPhongSpecular->setCamera(_camera);
  sphereTexturedPhongSpecular->setMaterial(materialSpherePhongSpecular);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -6.f, 3.f));
    sphereTexturedPhongSpecular->setModel(model);
  }
  _core->addDrawable(sphereTexturedPhongSpecular);

  // TODO: color is not so bright in comparison with cube
  // sphere PBR
  auto sphereColorPBR = std::make_shared<Texture>(
      _core->getResourceManager()->loadImage({"../assets/rustediron2_basecolor.png"}),
      settings->getLoadTextureAuxilaryFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
      state);
  auto sphereNormalPBR = std::make_shared<Texture>(
      _core->getResourceManager()->loadImage({"../assets/rustediron2_normal.png"}),
      settings->getLoadTextureAuxilaryFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
      state);
  auto sphereMetallicPBR = std::make_shared<Texture>(
      _core->getResourceManager()->loadImage({"../assets/rustediron2_metallic.png"}),
      settings->getLoadTextureAuxilaryFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
      state);
  auto sphereRoughnessPBR = std::make_shared<Texture>(
      _core->getResourceManager()->loadImage({"../assets/rustediron2_roughness.png"}),
      settings->getLoadTextureAuxilaryFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
      state);
  auto materialSpherePBR = std::make_shared<MaterialPBR>(commandBufferTransfer, state);
  // material can't have default state because it can be either cubemap or texture2D
  materialSpherePBR->setBaseColor(sphereColorPBR);
  materialSpherePBR->setNormal(sphereNormalPBR);
  materialSpherePBR->setMetallic(sphereMetallicPBR);
  materialSpherePBR->setRoughness(sphereRoughnessPBR);
  materialSpherePBR->setEmissive(_core->getResourceManager()->getTextureZero());
  materialSpherePBR->setOccluded(_core->getResourceManager()->getTextureZero());
  materialSpherePBR->setDiffuseIBL(_core->getResourceManager()->getCubemapZero()->getTexture());
  materialSpherePBR->setSpecularIBL(_core->getResourceManager()->getCubemapZero()->getTexture(),
                                    _core->getResourceManager()->getTextureZero());

  auto sphereTexturedPBR = std::make_shared<Shape3D>(
      ShapeType::SPHERE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  sphereTexturedPBR->setCamera(_camera);
  sphereTexturedPBR->setMaterial(materialSpherePBR);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 6.f, 3.f));
    sphereTexturedPBR->setModel(model);
  }
  _core->addDrawable(sphereTexturedPBR);

  // cube PBR wireframe
  auto sphereWireframePBR = std::make_shared<Shape3D>(
      ShapeType::SPHERE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  sphereWireframePBR->setCamera(_camera);
  sphereWireframePBR->setMaterial(materialSpherePBR);
  sphereWireframePBR->setDrawType(DrawType::WIREFRAME);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 6.f, 3.f));
    sphereWireframePBR->setModel(model);
  }
  _core->addDrawable(sphereWireframePBR);

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
  auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -3.f));
  model = glm::rotate(model, glm::radians(i), glm::vec3(1.f, 0.f, 0.f));
  _cubeTextured->setModel(model);

  model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 0.f, -3.f));
  model = glm::rotate(model, glm::radians(i), glm::vec3(1.f, 0.f, 0.f));
  _cubeTexturedWireframe->setModel(model);

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
  _core->getGUI()->startWindow("Help", {20, 20}, {widthScreen / 10, 0});
  _core->getGUI()->drawText({"Limited FPS: " + std::to_string(FPSLimited)});
  _core->getGUI()->drawText({"Maximum FPS: " + std::to_string(FPSReal)});
  _core->getGUI()->drawText({"Press 'c' to turn cursor on/off"});
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