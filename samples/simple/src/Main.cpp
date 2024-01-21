#include <iostream>
#include <chrono>
#include <future>
#include "Main.h"
#include "Shape3D.h"

std::shared_ptr<Shape3D> cubeTextured, cubeTexturedWireframe;
std::shared_ptr<PointLight> pointLightVertical, pointLightHorizontal;
std::shared_ptr<DirectionalLight> directionalLight;
std::shared_ptr<Shape3D> cubeColoredLightVertical, cubeColoredLightHorizontal;

Main::Main() {
  auto settings = std::make_shared<Settings>();
  settings->setName("Vulkan");
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
  commandBufferTransfer->beginCommands(0);
  auto state = _core->getState();
  _camera = std::make_shared<CameraFly>(settings);
  _camera->setProjectionParameters(60.f, 0.1f, 100.f);
  _core->getState()->getInput()->subscribe(std::dynamic_pointer_cast<InputSubscriber>(_camera));

  auto lightManager = _core->getLightManager();
  pointLightVertical = lightManager->createPointLight(settings->getDepthResolution());
  pointLightVertical->setColor(glm::vec3(1.f, 1.f, 1.f));
  pointLightHorizontal = lightManager->createPointLight(settings->getDepthResolution());
  pointLightHorizontal->setColor(glm::vec3(1.f, 1.f, 1.f));
  directionalLight = lightManager->createDirectionalLight(settings->getDepthResolution());
  directionalLight->setColor(glm::vec3(1.f, 1.f, 1.f));
  directionalLight->setPosition(glm::vec3(0.f, 20.f, 0.f));
  // TODO: rename setCenter to lookAt
  //  looking to (0.f, 0.f, 0.f) with up vector (0.f, 0.f, -1.f)
  directionalLight->setCenter({0.f, 0.f, 0.f});
  directionalLight->setUp({0.f, 0.f, -1.f});

  // cube colored light
  cubeColoredLightVertical = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  cubeColoredLightVertical->setCamera(_camera);
  cubeColoredLightVertical->getMesh()->setColor(
      std::vector{cubeColoredLightVertical->getMesh()->getVertexData().size(), glm::vec3(1.f, 1.f, 1.f)},
      commandBufferTransfer);
  _core->addDrawable(cubeColoredLightVertical);

  cubeColoredLightHorizontal = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  cubeColoredLightHorizontal->setCamera(_camera);
  cubeColoredLightHorizontal->getMesh()->setColor(
      std::vector{cubeColoredLightHorizontal->getMesh()->getVertexData().size(), glm::vec3(1.f, 1.f, 1.f)},
      commandBufferTransfer);
  _core->addDrawable(cubeColoredLightHorizontal);

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
      std::vector<std::string>{"../assets/right.jpg", "../assets/left.jpg", "../assets/top.jpg", "../assets/bottom.jpg",
                               "../assets/front.jpg", "../assets/back.jpg"},
      settings->getLoadTextureColorFormat(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);
  auto materialCubeTextured = std::make_shared<MaterialColor>(commandBufferTransfer, state);
  materialCubeTextured->setBaseColor(cubemap->getTexture());
  cubeTextured = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  cubeTextured->setCamera(_camera);
  cubeTextured->setMaterial(materialCubeTextured);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -3.f));
    cubeTextured->setModel(model);
  }
  _core->addDrawable(cubeTextured);

  // TODO: don't load the same texture 6 times for cubemap
  //  cube Phong
  auto cubemapColorPhong = std::make_shared<Cubemap>(
      std::vector<std::string>{"../assets/brickwall.jpg", "../assets/brickwall.jpg", "../assets/brickwall.jpg",
                               "../assets/brickwall.jpg", "../assets/brickwall.jpg", "../assets/brickwall.jpg"},
      settings->getLoadTextureColorFormat(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);
  auto cubemapNormalPhong = std::make_shared<Cubemap>(
      std::vector<std::string>{"../assets/brickwall_normal.jpg", "../assets/brickwall_normal.jpg",
                               "../assets/brickwall_normal.jpg", "../assets/brickwall_normal.jpg",
                               "../assets/brickwall_normal.jpg", "../assets/brickwall_normal.jpg"},
      settings->getLoadTextureColorFormat(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);
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
  cubeTexturedWireframe = std::make_shared<Shape3D>(
      ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  cubeTexturedWireframe->setCamera(_camera);
  cubeTexturedWireframe->setDrawType(DrawType::WIREFRAME);
  cubeTexturedWireframe->setMaterial(materialCubeTextured);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 0.f, -3.f));
    cubeTexturedWireframe->setModel(model);
  }
  _core->addDrawable(cubeTexturedWireframe);

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
      std::vector<std::string>{"../assets/brickwall_normal.jpg", "../assets/brickwall_normal.jpg",
                               "../assets/brickwall_normal.jpg", "../assets/brickwall_normal.jpg",
                               "../assets/brickwall_normal.jpg", "../assets/brickwall_normal.jpg"},
      settings->getLoadTextureColorFormat(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);
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
      std::vector<std::string>{"../assets/container.png", "../assets/container.png", "../assets/container.png",
                               "../assets/container.png", "../assets/container.png", "../assets/container.png"},
      settings->getLoadTextureColorFormat(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);
  auto cubemapSpecularPhong = std::make_shared<Cubemap>(
      std::vector<std::string>{"../assets/container_specular.png", "../assets/container_specular.png",
                               "../assets/container_specular.png", "../assets/container_specular.png",
                               "../assets/container_specular.png", "../assets/container_specular.png"},
      settings->getLoadTextureColorFormat(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);
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
  auto cubemapColorPBR = std::make_shared<Cubemap>(
      std::vector<std::string>{"../assets/rustediron2_basecolor.png", "../assets/rustediron2_basecolor.png",
                               "../assets/rustediron2_basecolor.png", "../assets/rustediron2_basecolor.png",
                               "../assets/rustediron2_basecolor.png", "../assets/rustediron2_basecolor.png"},
      settings->getLoadTextureColorFormat(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);
  auto cubemapNormalPBR = std::make_shared<Cubemap>(
      std::vector<std::string>{"../assets/rustediron2_normal.png", "../assets/rustediron2_normal.png",
                               "../assets/rustediron2_normal.png", "../assets/rustediron2_normal.png",
                               "../assets/rustediron2_normal.png", "../assets/rustediron2_normal.png"},
      settings->getLoadTextureColorFormat(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);
  auto cubemapMetallicPBR = std::make_shared<Cubemap>(
      std::vector<std::string>{"../assets/rustediron2_metallic.png", "../assets/rustediron2_metallic.png",
                               "../assets/rustediron2_metallic.png", "../assets/rustediron2_metallic.png",
                               "../assets/rustediron2_metallic.png", "../assets/rustediron2_metallic.png"},
      settings->getLoadTextureColorFormat(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);
  auto cubemapRoughnessPBR = std::make_shared<Cubemap>(
      std::vector<std::string>{"../assets/rustediron2_roughness.png", "../assets/rustediron2_roughness.png",
                               "../assets/rustediron2_roughness.png", "../assets/rustediron2_roughness.png",
                               "../assets/rustediron2_roughness.png", "../assets/rustediron2_roughness.png"},
      settings->getLoadTextureColorFormat(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);
  auto materialCubePBR = std::make_shared<MaterialPBR>(commandBufferTransfer, state);
  // material can't have default state because it can be either cubemap or texture2D
  materialCubePBR->setBaseColor(cubemapColorPhong->getTexture());
  materialCubePBR->setNormal(cubemapNormalPhong->getTexture());
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

  // sphere textured
  auto sphereTexture = std::make_shared<Texture>("../assets/right.jpg", settings->getLoadTextureAuxilaryFormat(),
                                                 VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer, state);
  auto materialSphereTextured = std::make_shared<MaterialColor>(commandBufferTransfer, state);
  materialSphereTextured->setBaseColor(sphereTexture);
  auto sphereTextured = std::make_shared<Shape3D>(
      ShapeType::SPHERE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  sphereTextured->setCamera(_camera);
  sphereTextured->setMaterial(materialSphereTextured);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 0.f, 3.f));
    sphereTextured->setModel(model);
  }
  _core->addDrawable(sphereTextured);

  // sphere colored
  auto sphereColored = std::make_shared<Shape3D>(
      ShapeType::SPHERE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  sphereColored->setCamera(_camera);
  sphereColored->getMesh()->setColor(
      std::vector{sphereColored->getMesh()->getVertexData().size(), glm::vec3(0.f, 1.f, 0.f)}, commandBufferTransfer);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 3.f, 3.f));
    sphereColored->setModel(model);
  }
  _core->addDrawable(sphereColored);

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
}

void Main::update() {
  static float i = 0;
  auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -3.f));
  model = glm::rotate(model, glm::radians(i), glm::vec3(1.f, 0.f, 0.f));
  cubeTextured->setModel(model);

  model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 0.f, -3.f));
  model = glm::rotate(model, glm::radians(i), glm::vec3(1.f, 0.f, 0.f));
  cubeTexturedWireframe->setModel(model);

  // update light position
  float radius = 15.f;
  static float angleHorizontal = 90.f;
  glm::vec3 lightPositionHorizontal = glm::vec3(radius * cos(glm::radians(angleHorizontal)), radius,
                                                radius * sin(glm::radians(angleHorizontal)));
  static float angleVertical = 0.f;
  glm::vec3 lightPositionVertical = glm::vec3(0.f, radius * sin(glm::radians(angleVertical)),
                                              radius * cos(glm::radians(angleVertical)));

  pointLightVertical->setPosition(lightPositionVertical);
  {
    auto model = glm::translate(glm::mat4(1.f), lightPositionVertical);
    model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f));
    cubeColoredLightVertical->setModel(model);
  }
  pointLightHorizontal->setPosition(lightPositionHorizontal);
  {
    auto model = glm::translate(glm::mat4(1.f), lightPositionHorizontal);
    model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f));
    cubeColoredLightHorizontal->setModel(model);
  }

  i += 0.1f;
  angleHorizontal += 0.05f;
  angleVertical += 0.1f;
}

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