#include <iostream>
#include <chrono>
#include <future>
#include "Main.h"
#include <random>
#include <glm/gtc/random.hpp>

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
  _core->setCamera(_camera);
  auto particleTexture = std::make_shared<Texture>(_core->getResourceManager()->loadImage({"../assets/gradient.png"}),
                                                   settings->getLoadTextureAuxilaryFormat(),
                                                   VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer, state);
  std::default_random_engine rndEngine((unsigned)time(nullptr));
  std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);
  {
    // Initial particle positions on a circle
    std::vector<Particle> particles(300);
    float r = 0.1f;
    for (auto& particle : particles) {
      particle.position = glm::sphericalRand(r);
      particle.radius = r;

      particle.minColor = glm::vec4(0.9f, 0.4f, 0.2f, 1.f);
      particle.maxColor = glm::vec4(1.f, 0.5f, 0.3f, 1.f);
      particle.color = glm::vec4(
          particle.minColor.r + (particle.maxColor.r - particle.minColor.r) * rndDist(rndEngine),
          particle.minColor.g + (particle.maxColor.g - particle.minColor.g) * rndDist(rndEngine),
          particle.minColor.b + (particle.maxColor.b - particle.minColor.b) * rndDist(rndEngine),
          particle.minColor.a + (particle.maxColor.a - particle.minColor.a) * rndDist(rndEngine));

      particle.minLife = 0.f;
      particle.maxLife = 1.f;
      particle.life = particle.minLife + (particle.maxLife - particle.minLife) * rndDist(rndEngine);
      particle.iteration = -1;

      particle.minVelocity = 0.f;
      particle.maxVelocity = 1.f;
      particle.velocity = particle.minVelocity + (particle.maxVelocity - particle.minVelocity) * rndDist(rndEngine);
      particle.velocityDirection = glm::vec3(0.f, 1.f, 0.f);
    }

    auto particleSystem = std::make_shared<ParticleSystem>(particles, particleTexture, commandBufferTransfer, state);
    {
      auto matrix = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, 2.f));
      matrix = glm::scale(matrix, glm::vec3(0.5f, 0.5f, 0.5f));

      particleSystem->setModel(matrix);
    }
    _core->addParticleSystem(particleSystem);
  }
  {
    // Initial particle positions on a circle
    std::vector<Particle> particles(360);
    float r = 0.05f;
    for (int i = 0; i < particles.size(); i++) {
      auto& particle = particles[i];
      particle.position = glm::sphericalRand(r);
      particle.radius = r;

      particle.minColor = glm::vec4(0.2f, 0.4f, 0.9f, 1.f);
      particle.maxColor = glm::vec4(0.3, 0.5f, 1.f, 1.f);
      particle.color = glm::vec4(
          particle.minColor.r + (particle.maxColor.r - particle.minColor.r) * rndDist(rndEngine),
          particle.minColor.g + (particle.maxColor.g - particle.minColor.g) * rndDist(rndEngine),
          particle.minColor.b + (particle.maxColor.b - particle.minColor.b) * rndDist(rndEngine),
          particle.minColor.a + (particle.maxColor.a - particle.minColor.a) * rndDist(rndEngine));

      particle.minLife = 0.f;
      particle.maxLife = 1.f;
      particle.life = particle.minLife + (particle.maxLife - particle.minLife) * rndDist(rndEngine);
      particle.iteration = -1.f;
      particle.minVelocity = 0.f;
      particle.maxVelocity = 1.f;
      particle.velocity = particle.minVelocity + (particle.maxVelocity - particle.minVelocity) * rndDist(rndEngine);

      glm::vec3 lightPositionHorizontal = glm::vec3(cos(glm::radians((float)i)), 0, sin(glm::radians((float)i)));
      particle.velocityDirection = glm::normalize(lightPositionHorizontal);
    }
    auto particleSystem = std::make_shared<ParticleSystem>(particles, particleTexture, commandBufferTransfer, state);
    {
      auto matrix = glm::translate(glm::mat4(1.f), glm::vec3(0.5f, 0.f, 2.f));
      matrix = glm::scale(matrix, glm::vec3(0.5f, 0.5f, 0.5f));

      particleSystem->setModel(matrix);
    }
    _core->addParticleSystem(particleSystem);
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