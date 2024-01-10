#include <iostream>
#include <chrono>
#include <future>
#include "Main.h"
#include "Sphere.h"

std::shared_ptr<Cube> cubeTextured, cubeTexturedPhong;

Main::Main() {
  auto settings = std::make_shared<Settings>();
  settings->setName("Vulkan");
  settings->setResolution(std::tuple{1600, 900});
  // for HDR, linear 16 bit per channel to represent values outside of 0-1 range (UNORM - float [0, 1], SFLOAT - float)
  // https://registry.khronos.org/vulkan/specs/1.1/html/vkspec.html#_identification_of_formats
  settings->setGraphicColorFormat(VK_FORMAT_R32G32B32A32_SFLOAT);
  settings->setSwapchainColorFormat(VK_FORMAT_B8G8R8A8_UNORM);
  // SRGB the same as UNORM but + gamma conversion
  settings->setLoadTextureColorFormat(VK_FORMAT_R8G8B8A8_SRGB);
  settings->setLoadTextureAuxilaryFormat(VK_FORMAT_R8G8B8A8_UNORM);
  settings->setAnisotropicSamples(0);
  settings->setDepthFormat(VK_FORMAT_D32_SFLOAT);
  settings->setMaxFramesInFlight(2);
  settings->setThreadsInPool(6);
  settings->setDesiredFPS(1000);

  _core = std::make_shared<Core>(settings);

  auto lightManager = _core->getLightManager();
  auto commandBufferTransfer = _core->getCommandBufferTransfer();
  commandBufferTransfer->beginCommands(0);
  auto state = _core->getState();
  _camera = std::make_shared<CameraFly>(settings);
  _camera->setProjectionParameters(60.f, 0.1f, 100.f);
  _core->getState()->getInput()->subscribe(std::dynamic_pointer_cast<InputSubscriber>(_camera));

  // cube textured
  auto cubemap = std::make_shared<Cubemap>(
      std::vector<std::string>{"../assets/right.jpg", "../assets/left.jpg", "../assets/top.jpg", "../assets/bottom.jpg",
                               "../assets/front.jpg", "../assets/back.jpg"},
      settings->getLoadTextureColorFormat(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);
  auto materialCubeTextured = std::make_shared<MaterialColor>(commandBufferTransfer, state);
  materialCubeTextured->setBaseColor(cubemap->getTexture());
  cubeTextured = std::make_shared<Cube>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, VK_CULL_MODE_NONE,
      lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  cubeTextured->setCamera(_camera);
  cubeTextured->setMaterial(materialCubeTextured);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -3.f));
    cubeTextured->setModel(model);
  }
  _core->addDrawable(cubeTextured);

  // cube colored
  auto cubeColored = std::make_shared<Cube>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, VK_CULL_MODE_NONE,
      lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  cubeColored->setCamera(_camera);
  cubeColored->getMesh()->setColor(
      std::vector{cubeColored->getMesh()->getVertexData().size(), glm::vec3(1.f, 0.f, 0.f)}, commandBufferTransfer);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 3.f, -3.f));
    cubeColored->setModel(model);
  }
  _core->addDrawable(cubeColored);

  // cube Phong
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

  cubeTexturedPhong = std::make_shared<Cube>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, VK_CULL_MODE_NONE,
      lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  cubeTexturedPhong->setCamera(_camera);
  cubeTexturedPhong->setMaterial(materialCubePhong);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -3.f));
    cubeTexturedPhong->setModel(model);
  }
  _core->addDrawable(cubeTexturedPhong);

  // sphere textured
  auto sphereTexture = std::make_shared<Texture>("../assets/right.jpg", settings->getLoadTextureAuxilaryFormat(),
                                                 VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer, state);
  auto materialSphereTextured = std::make_shared<MaterialColor>(commandBufferTransfer, state);
  materialSphereTextured->setBaseColor(sphereTexture);
  auto sphereTextured = std::make_shared<Sphere>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, VK_CULL_MODE_BACK_BIT,
      lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  sphereTextured->setCamera(_camera);
  sphereTextured->setMaterial(materialSphereTextured);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 0.f, -3.f));
    sphereTextured->setModel(model);
  }
  _core->addDrawable(sphereTextured);

  // sphere colored
  auto sphereColored = std::make_shared<Sphere>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, VK_CULL_MODE_BACK_BIT,
      lightManager, commandBufferTransfer, _core->getResourceManager(), state);
  sphereColored->setCamera(_camera);
  sphereColored->getMesh()->setColor(
      std::vector{sphereColored->getMesh()->getVertexData().size(), glm::vec3(0.f, 1.f, 0.f)}, commandBufferTransfer);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 3.f, -3.f));
    sphereColored->setModel(model);
  }
  _core->addDrawable(sphereColored);

  commandBufferTransfer->endCommands();
  commandBufferTransfer->submitToQueue(true);

  _core->registerUpdate(std::bind(&Main::update, this));
}

void Main::update() {
  static float i = 0;
  auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -3.f));
  model = glm::rotate(model, glm::radians(i), glm::vec3(1.f, 0.f, 0.f));
  cubeTextured->setModel(model);
  i += 0.1f;
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