#include "Main.h"
#include "IBL.h"
#include "Equirectangular.h"
#include <random>
#include <glm/gtc/random.hpp>

void InputHandler::cursorNotify(std::any window, float xPos, float yPos) {}

void InputHandler::mouseNotify(std::any window, int button, int action, int mods) {}

void InputHandler::keyNotify(std::any window, int key, int scancode, int action, int mods) {
#ifndef __ANDROID__
  if ((action == GLFW_RELEASE && key == GLFW_KEY_C)) {
    if (_cursorEnabled) {
      glfwSetInputMode(std::any_cast<GLFWwindow*>(window), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      _cursorEnabled = false;
    } else {
      glfwSetInputMode(std::any_cast<GLFWwindow*>(window), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      _cursorEnabled = true;
    }
  }
#endif
}

void InputHandler::charNotify(std::any window, unsigned int code) {}

void InputHandler::scrollNotify(std::any window, double xOffset, double yOffset) {}

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

  // start transfer command buffer
  auto commandBufferTransfer = _core->getCommandBufferTransfer();
  commandBufferTransfer->beginCommands();
  auto state = _core->getState();
  _camera = std::make_shared<CameraFly>(settings);
  _camera->setProjectionParameters(60.f, 0.1f, 100.f);
  _core->getState()->getInput()->subscribe(std::dynamic_pointer_cast<InputSubscriber>(_camera));
  _inputHandler = std::make_shared<InputHandler>();
  _core->getState()->getInput()->subscribe(std::dynamic_pointer_cast<InputSubscriber>(_inputHandler));
  _core->setCamera(_camera);

  auto gui = _core->getGUI();
  auto postprocessing = _core->getPostprocessing();

  _pointLightHorizontal = _core->createPointLight(settings->getDepthResolution());
  _pointLightHorizontal->setColor(glm::vec3(1.f, 1.f, 1.f));
  _pointLightHorizontal->setPosition({3.f, 4.f, 0.f});

  auto ambientLight = _core->createAmbientLight();
  // calculate ambient color with default gamma
  float ambientColor = std::pow(0.05f, postprocessing->getGamma());
  ambientLight->setColor(glm::vec3(ambientColor, ambientColor, ambientColor));

  _directionalLight = _core->createDirectionalLight(settings->getDepthResolution());
  _directionalLight->setColor(glm::vec3(1.f, 1.f, 1.f));
  _directionalLight->setPosition({0.f, 35.f, 0.f});
  _directionalLight->setCenter({0.f, 0.f, 0.f});
  _directionalLight->setUp({0.f, 0.f, -1.f});

  _debugVisualization = std::make_shared<DebugVisualization>(_camera, _core);
  _core->getState()->getInput()->subscribe(std::dynamic_pointer_cast<InputSubscriber>(_debugVisualization));

  auto cameraSetLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
  cameraSetLayout->createUniformBuffer();

  {
    auto texture = _core->createTexture("../../sprite/assets/brickwall.jpg", settings->getLoadTextureColorFormat(), 1);
    auto normalMap = _core->createTexture("../../sprite/assets/brickwall_normal.jpg",
                                          settings->getLoadTextureAuxilaryFormat(), 1);

    auto material = _core->createMaterialPhong(MaterialTarget::SIMPLE);
    material->setBaseColor({texture});
    material->setNormal({normalMap});
    material->setSpecular({_core->getResourceManager()->getTextureZero()});

    auto spriteForward = _core->createSprite();
    spriteForward->setMaterial(material);
    {
      glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, 1.f));
      spriteForward->setModel(model);
    }
    _core->addDrawable(spriteForward);

    auto spriteBackward = _core->createSprite();
    spriteBackward->setMaterial(material);
    {
      auto model = glm::rotate(glm::mat4(1.f), glm::radians(180.f), glm::vec3(0.f, 1.f, 0.f));
      spriteBackward->setModel(model);
    }
    _core->addDrawable(spriteBackward);

    auto spriteLeft = _core->createSprite();
    spriteLeft->setMaterial(material);
    {
      glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(-0.5f, 0.f, 0.5f));
      model = glm::rotate(model, glm::radians(-90.f), glm::vec3(0.f, 1.f, 0.f));
      spriteLeft->setModel(model);
    }
    _core->addDrawable(spriteLeft);

    auto spriteRight = _core->createSprite();
    spriteRight->setMaterial(material);
    {
      glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(0.5f, 0.f, 0.5f));
      model = glm::rotate(model, glm::radians(90.f), glm::vec3(0.f, 1.f, 0.f));
      spriteRight->setModel(model);
    }
    _core->addDrawable(spriteRight);

    auto spriteTop = _core->createSprite();
    spriteTop->setMaterial(material);
    {
      glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(0.0f, 0.5f, 0.5f));
      model = glm::rotate(model, glm::radians(-90.f), glm::vec3(1.f, 0.f, 0.f));
      spriteTop->setModel(model);
    }
    _core->addDrawable(spriteTop);

    auto spriteBot = _core->createSprite();
    spriteBot->setMaterial(material);
    {
      glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(0.0f, -0.5f, 0.5f));
      model = glm::rotate(model, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
      spriteBot->setModel(model);
    }
    _core->addDrawable(spriteBot);
  }
  auto modelGLTF = _core->createModelGLTF("../../IBL/assets/DamagedHelmet/DamagedHelmet.gltf");
  auto modelGLTFPhong = _core->createModel3D(modelGLTF);
  modelGLTFPhong->setMaterial(modelGLTF->getMaterialsPhong());
  {
    glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(-2.f, 2.f, -5.f));
    model = glm::scale(model, glm::vec3(1.f, 1.f, 1.f));
    modelGLTFPhong->setModel(model);
  }
  _core->addDrawable(modelGLTFPhong);
  _core->addShadowable(modelGLTFPhong);

  auto modelGLTFPBR = _core->createModel3D(modelGLTF);
  auto pbrMaterial = modelGLTF->getMaterialsPBR();
  modelGLTFPBR->setMaterial(pbrMaterial);
  {
    glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(-2.f, 2.f, -3.f));
    model = glm::scale(model, glm::vec3(1.f, 1.f, 1.f));
    modelGLTFPBR->setModel(model);
  }
  _core->addDrawable(modelGLTFPBR);
  _core->addShadowable(modelGLTFPBR);

  auto tile0 = _core->createTexture("../assets/Terrain/dirt.jpg", settings->getLoadTextureColorFormat(), 6);
  auto tile1 = _core->createTexture("../assets/Terrain/grass.jpg", settings->getLoadTextureColorFormat(), 6);
  auto tile2 = _core->createTexture("../assets/Terrain/rock_gray.png", settings->getLoadTextureColorFormat(), 6);
  auto tile3 = _core->createTexture("../assets/Terrain/snow.png", settings->getLoadTextureColorFormat(), 6);

  auto terrain = _core->createTerrain("../assets/Terrain/heightmap.png", std::pair{12, 12});
  auto materialTerrain = _core->createMaterialPhong(MaterialTarget::TERRAIN);
  materialTerrain->setBaseColor({tile0, tile1, tile2, tile3});
  auto fillMaterialTerrainPhong = [core = _core](std::shared_ptr<MaterialPhong> material) {
    if (material->getBaseColor().size() == 0)
      material->setBaseColor(std::vector{4, core->getResourceManager()->getTextureOne()});
    if (material->getNormal().size() == 0)
      material->setNormal(std::vector{4, core->getResourceManager()->getTextureZero()});
    if (material->getSpecular().size() == 0)
      material->setSpecular(std::vector{4, core->getResourceManager()->getTextureZero()});
  };
  fillMaterialTerrainPhong(materialTerrain);
  terrain->setMaterial(materialTerrain);
  {
    auto scaleMatrix = glm::scale(glm::mat4(1.f), glm::vec3(0.1f, 0.1f, 0.1f));
    auto translateMatrix = glm::translate(scaleMatrix, glm::vec3(2.f, -6.f, 0.f));
    terrain->setModel(translateMatrix);
  }
  _core->addDrawable(terrain, AlphaType::OPAQUE);
  _core->addShadowable(terrain);

  auto particleTexture = _core->createTexture("../../Particles/assets/gradient.png",
                                              settings->getLoadTextureAuxilaryFormat(), 1);

  std::default_random_engine rndEngine((unsigned)time(nullptr));
  std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);
  // Initial particle positions on a circle
  std::vector<Particle> particles(360);
  float r = 0.05f;
  for (int i = 0; i < particles.size(); i++) {
    auto& particle = particles[i];
    particle.position = glm::sphericalRand(r);
    particle.radius = r;

    particle.minColor = glm::vec4(0.2f, 0.4f, 0.9f, 1.f);
    particle.maxColor = glm::vec4(0.3, 0.5f, 1.f, 1.f);
    particle.color = glm::vec4(particle.minColor.r + (particle.maxColor.r - particle.minColor.r) * rndDist(rndEngine),
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
  auto particleSystem = _core->createParticleSystem(particles, particleTexture);
  {
    auto matrix = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, 2.f));
    matrix = glm::scale(matrix, glm::vec3(0.5f, 0.5f, 0.5f));

    particleSystem->setModel(matrix);
  }
  _core->addParticleSystem(particleSystem);

  std::vector<std::shared_ptr<Shape3D>> spheres(6);
  // non HDR
  spheres[0] = _core->createShape3D(ShapeType::SPHERE);
  spheres[0]->getMesh()->setColor({{0.f, 0.f, 0.1f}}, commandBufferTransfer);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 5.f, 0.f));
    spheres[0]->setModel(model);
  }
  spheres[1] = _core->createShape3D(ShapeType::SPHERE);
  spheres[1]->getMesh()->setColor({{0.f, 0.f, 0.5f}}, commandBufferTransfer);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(2.f, 5.f, 0.f));
    spheres[1]->setModel(model);
  }
  spheres[2] = _core->createShape3D(ShapeType::SPHERE);
  spheres[2]->getMesh()->setColor({{0.f, 0.f, 10.f}}, commandBufferTransfer);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(-2.f, 5.f, 0.f));
    spheres[2]->setModel(model);
  }
  // HDR
  spheres[3] = _core->createShape3D(ShapeType::SPHERE);
  spheres[3]->getMesh()->setColor({{5.f, 0.f, 0.f}}, commandBufferTransfer);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 5.f, 2.f));
    spheres[3]->setModel(model);
  }
  spheres[4] = _core->createShape3D(ShapeType::SPHERE);
  spheres[4]->getMesh()->setColor({{0.f, 5.f, 0.f}}, commandBufferTransfer);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(-4.f, 7.f, -2.f));
    spheres[4]->setModel(model);
  }
  spheres[5] = _core->createShape3D(ShapeType::SPHERE);
  spheres[5]->getMesh()->setColor({{0.f, 0.f, 20.f}}, commandBufferTransfer);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(-4.f, 5.f, -2.f));
    spheres[5]->setModel(model);
  }

  for (auto& sphere : spheres) {
    _core->addDrawable(sphere);
  }

  auto [width, height] = settings->getResolution();
  auto cubemap = _core->createCubemap(
      {"../assets/Skybox/right.jpg", "../assets/Skybox/left.jpg", "../assets/Skybox/top.jpg",
       "../assets/Skybox/bottom.jpg", "../assets/Skybox/front.jpg", "../assets/Skybox/back.jpg"},
      settings->getLoadTextureColorFormat(), 1);

  auto cube = _core->createShape3D(ShapeType::CUBE);
  auto materialColor = _core->createMaterialColor(MaterialTarget::SIMPLE);
  materialColor->setBaseColor({cubemap->getTexture()});
  cube->setMaterial(materialColor);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 3.f, 0.f));
    cube->setModel(model);
  }
  _core->addDrawable(cube);

  auto ibl = _core->createIBL();

  auto equiCube = _core->createShape3D(ShapeType::CUBE, VK_CULL_MODE_NONE);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 3.f, -3.f));
    equiCube->setModel(model);
  }
  _core->addDrawable(equiCube);

  auto diffuseCube = _core->createShape3D(ShapeType::CUBE, VK_CULL_MODE_NONE);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(2.f, 3.f, -3.f));
    diffuseCube->setModel(model);
  }
  _core->addDrawable(diffuseCube);

  auto specularCube = _core->createShape3D(ShapeType::CUBE, VK_CULL_MODE_NONE);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(4.f, 3.f, -3.f));
    specularCube->setModel(model);
  }
  _core->addDrawable(specularCube);

  auto equirectangular = _core->createEquirectangular("../assets/Skybox/newport_loft.hdr");
  auto cubemapConverted = equirectangular->getCubemap();
  auto materialColorEq = _core->createMaterialColor(MaterialTarget::SIMPLE);
  materialColorEq->setBaseColor({cubemapConverted->getTexture()});
  ibl->setMaterial(materialColorEq);
  auto materialColorCM = _core->createMaterialColor(MaterialTarget::SIMPLE);
  auto materialColorDiffuse = _core->createMaterialColor(MaterialTarget::SIMPLE);
  auto materialColorSpecular = _core->createMaterialColor(MaterialTarget::SIMPLE);
  materialColorCM->setBaseColor({cubemapConverted->getTexture()});
  equiCube->setMaterial(materialColorCM);

  ibl->drawDiffuse();
  ibl->drawSpecular();
  ibl->drawSpecularBRDF();

  // display specular as texture
  materialColorSpecular->setBaseColor({ibl->getCubemapSpecular()->getTexture()});
  specularCube->setMaterial(materialColorSpecular);

  // display diffuse as texture
  materialColorDiffuse->setBaseColor({ibl->getCubemapDiffuse()->getTexture()});
  diffuseCube->setMaterial(materialColorDiffuse);

  // set diffuse to material
  for (auto& material : pbrMaterial) {
    material->setDiffuseIBL(ibl->getCubemapDiffuse()->getTexture());
  }

  // set specular to material
  for (auto& material : pbrMaterial) {
    material->setSpecularIBL(ibl->getCubemapSpecular()->getTexture(), ibl->getTextureSpecularBRDF());
  }

  auto materialBRDF = _core->createMaterialPhong(MaterialTarget::SIMPLE);
  materialBRDF->setBaseColor({ibl->getTextureSpecularBRDF()});
  materialBRDF->setNormal({_core->getResourceManager()->getTextureZero()});
  materialBRDF->setSpecular({_core->getResourceManager()->getTextureZero()});
  materialBRDF->setCoefficients(glm::vec3(1.f), glm::vec3(0.f), glm::vec3(0.f), 0.f);
  auto spriteBRDF = _core->createSprite();
  spriteBRDF->enableLighting(false);
  spriteBRDF->enableShadow(false);
  spriteBRDF->enableDepth(false);
  spriteBRDF->setMaterial(materialBRDF);
  {
    glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(5.f, 3.f, 1.f));
    spriteBRDF->setModel(model);
  }
  _core->addDrawable(spriteBRDF);

  _core->endRecording();

  _core->registerUpdate(std::bind(&Main::update, this));
  // can be lambda passed that calls reset
  _core->registerReset(std::bind(&Main::reset, this, std::placeholders::_1, std::placeholders::_2));
}

void Main::update() {
  // update light position
  float radius = 15.f;
  static float angleHorizontal = 90.f;
  glm::vec3 lightPositionHorizontal = glm::vec3(radius * cos(glm::radians(angleHorizontal)), radius,
                                                radius * sin(glm::radians(angleHorizontal)));
  _pointLightHorizontal->setPosition(lightPositionHorizontal);

  angleHorizontal += 0.05f;

  auto [FPSLimited, FPSReal] = _core->getFPS();
  auto [widthScreen, heightScreen] = _core->getState()->getSettings()->getResolution();
  _core->getGUI()->startWindow("Help", {20, 20}, {widthScreen / 10, 0});
  if (_core->getGUI()->startTree("Main")) {
    _core->getGUI()->drawText({"Limited FPS: " + std::to_string(FPSLimited)});
    _core->getGUI()->drawText({"Maximum FPS: " + std::to_string(FPSReal)});
    _core->getGUI()->drawText({"Press 'c' to turn cursor on/off"});
    _core->getGUI()->endTree();
  }
  if (_core->getGUI()->startTree("Debug")) {
    _debugVisualization->update();
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