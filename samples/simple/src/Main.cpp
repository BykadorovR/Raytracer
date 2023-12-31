#include <iostream>
#include <chrono>
#include <future>
#include "Main.h"

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

  auto cubemap = std::make_shared<Cubemap>(
      std::vector<std::string>{"../assets/right.jpg", "../assets/left.jpg", "../assets/top.jpg", "../assets/bottom.jpg",
                               "../assets/front.jpg", "../assets/back.jpg"},
      settings->getLoadTextureColorFormat(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);
  auto materialColor = std::make_shared<MaterialColor>(commandBufferTransfer, state);
  materialColor->setBaseColor(cubemap->getTexture());

  auto cube = std::make_shared<Cube>(std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
                                     VK_CULL_MODE_NONE, lightManager, commandBufferTransfer, state);
  _camera = std::make_shared<CameraFly>(settings);
  _camera->setProjectionParameters(60.f, 0.1f, 100.f);
  _core->getState()->getInput()->subscribe(std::dynamic_pointer_cast<InputSubscriber>(_camera));
  cube->setCamera(_camera);
  cube->setMaterial(materialColor);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -3.f));
    cube->setModel(model);
  }
  _core->addDrawable(cube);
  commandBufferTransfer->endCommands();
  commandBufferTransfer->submitToQueue(true);
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