#include <iostream>
#include <chrono>
#include <future>
#include "Main.h"
#include "Line.h"
#include "Sprite.h"
#include "Model.h"
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
  settings->setName("Shadow");
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

  auto lightManager = _core->getLightManager();
  _pointLightVertical = lightManager->createPointLight(settings->getDepthResolution());
  _pointLightVertical->setColor(glm::vec3(_pointVerticalValue, _pointVerticalValue, _pointVerticalValue));
  _pointLightHorizontal = lightManager->createPointLight(settings->getDepthResolution());
  _pointLightHorizontal->setColor(glm::vec3(_pointHorizontalValue, _pointHorizontalValue, _pointHorizontalValue));
  _directionalLight = lightManager->createDirectionalLight(settings->getDepthResolution());
  _directionalLight->setColor(glm::vec3(_directionalValue, _directionalValue, _directionalValue));
  _directionalLight->setPosition(glm::vec3(0.f, 20.f, 0.f));
  // TODO: rename setCenter to lookAt
  //  looking to (0.f, 0.f, 0.f) with up vector (0.f, 0.f, -1.f)
  _directionalLight->setCenter({0.f, 0.f, 0.f});
  _directionalLight->setUp({0.f, 0.f, -1.f});

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
    if (material->getBaseColor().size() == 0) material->setBaseColor({core->getResourceManager()->getTextureOne()});
    if (material->getNormal().size() == 0) material->setNormal({core->getResourceManager()->getTextureZero()});
    if (material->getSpecular().size() == 0) material->setSpecular({core->getResourceManager()->getTextureZero()});
  };

  auto fillMaterialPBR = [core = _core](std::shared_ptr<MaterialPBR> material) {
    if (material->getBaseColor().size() == 0) material->setBaseColor({core->getResourceManager()->getTextureOne()});
    if (material->getNormal().size() == 0) material->setNormal({core->getResourceManager()->getTextureZero()});
    if (material->getMetallic().size() == 0) material->setMetallic({core->getResourceManager()->getTextureZero()});
    if (material->getRoughness().size() == 0) material->setRoughness({core->getResourceManager()->getTextureZero()});
    if (material->getOccluded().size() == 0) material->setOccluded({core->getResourceManager()->getTextureZero()});
    if (material->getEmissive().size() == 0) material->setEmissive({core->getResourceManager()->getTextureZero()});
    material->setDiffuseIBL(core->getResourceManager()->getCubemapZero()->getTexture());
    material->setSpecularIBL(core->getResourceManager()->getCubemapZero()->getTexture(),
                             core->getResourceManager()->getTextureZero());
  };
  auto fillMaterialTerrainPhong = [core = _core](std::shared_ptr<MaterialPhong> material) {
    if (material->getBaseColor().size() == 0)
      material->setBaseColor(std::vector{4, core->getResourceManager()->getTextureOne()});
    if (material->getNormal().size() == 0)
      material->setNormal(std::vector{4, core->getResourceManager()->getTextureZero()});
    if (material->getSpecular().size() == 0)
      material->setSpecular(std::vector{4, core->getResourceManager()->getTextureZero()});
  };

  auto fillMaterialTerrainPBR = [core = _core](std::shared_ptr<MaterialPBR> material) {
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
    //  cube Phong
    auto cubemapColorPhong = std::make_shared<Cubemap>(
        _core->getResourceManager()->loadImage(
            std::vector<std::string>{"../../shape/assets/brickwall.jpg", "../../shape/assets/brickwall.jpg",
                                     "../../shape/assets/brickwall.jpg", "../../shape/assets/brickwall.jpg",
                                     "../../shape/assets/brickwall.jpg", "../../shape/assets/brickwall.jpg"}),
        settings->getLoadTextureColorFormat(), mipMapLevels, VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        commandBufferTransfer, state);
    auto cubemapNormalPhong = std::make_shared<Cubemap>(
        _core->getResourceManager()->loadImage(std::vector<std::string>{
            "../../shape/assets/brickwall_normal.jpg", "../../shape/assets/brickwall_normal.jpg",
            "../../shape/assets/brickwall_normal.jpg", "../../shape/assets/brickwall_normal.jpg",
            "../../shape/assets/brickwall_normal.jpg", "../../shape/assets/brickwall_normal.jpg"}),
        settings->getLoadTextureColorFormat(), mipMapLevels, VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        commandBufferTransfer, state);
    auto materialCubePhong = std::make_shared<MaterialPhong>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
    materialCubePhong->setBaseColor({cubemapColorPhong->getTexture()});
    materialCubePhong->setNormal({cubemapNormalPhong->getTexture()});
    materialCubePhong->setSpecular({_core->getResourceManager()->getCubemapZero()->getTexture()});

    auto cubeTexturedPhong = std::make_shared<Shape3D>(
        ShapeType::CUBE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
        VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
    cubeTexturedPhong->setMaterial(materialCubePhong);
    {
      auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -3.f, -3.f));
      cubeTexturedPhong->setModel(model);
    }
    _core->addDrawable(cubeTexturedPhong);
    _core->addShadowable(cubeTexturedPhong);
  }
  {
    // sphere colored
    auto sphereColored = std::make_shared<Shape3D>(
        ShapeType::SPHERE, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
        VK_CULL_MODE_BACK_BIT, lightManager, commandBufferTransfer, _core->getResourceManager(), state);
    sphereColored->getMesh()->setColor(
        std::vector{sphereColored->getMesh()->getVertexData().size(), glm::vec3(0.f, 1.f, 0.f)}, commandBufferTransfer);
    {
      auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -5.f));
      sphereColored->setModel(model);
    }
    _core->addDrawable(sphereColored);
    _core->addShadowable(sphereColored);
  }
  {
    auto gltfModelDancing = _core->getResourceManager()->loadModel("../../model/assets/BrainStem/BrainStem.gltf");
    auto modelDancing = std::make_shared<Model3D>(
        std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, gltfModelDancing->getNodes(),
        gltfModelDancing->getMeshes(), lightManager, commandBufferTransfer, _core->getResourceManager(), state);
    auto materialModelDancing = gltfModelDancing->getMaterialsPBR();
    for (auto& material : materialModelDancing) {
      fillMaterialPBR(material);
    }
    modelDancing->setMaterial(materialModelDancing);
    auto animationDancing = std::make_shared<Animation>(gltfModelDancing->getNodes(), gltfModelDancing->getSkins(),
                                                        gltfModelDancing->getAnimations(), state);
    // set animation to model, so joints will be passed to shader
    modelDancing->setAnimation(animationDancing);
    _core->addAnimation(animationDancing);
    {
      auto model = glm::translate(glm::mat4(1.f), glm::vec3(-5.f, -1.f, -3.f));
      model = glm::scale(model, glm::vec3(1.f, 1.f, 1.f));
      modelDancing->setModel(model);
    }
    _core->addDrawable(modelDancing);
    _core->addShadowable(modelDancing);
  }
  {
    auto tile0Color = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/desert/albedo.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile1Color = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/rock/albedo.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile2Color = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/grass/albedo.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile3Color = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/ground/albedo.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto terrainPhong = std::make_shared<Terrain>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/heightmap.png"}), std::pair{12, 12},
        std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, commandBufferTransfer,
        lightManager, state);
    auto materialTerrainPhong = std::make_shared<MaterialPhong>(MaterialTarget::TERRAIN, commandBufferTransfer, state);
    materialTerrainPhong->setBaseColor({tile0Color, tile1Color, tile2Color, tile3Color});
    fillMaterialTerrainPhong(materialTerrainPhong);
    terrainPhong->setMaterial(materialTerrainPhong);
    {
      auto translateMatrix = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -7.f, 0.f));
      auto scaleMatrix = glm::scale(translateMatrix, glm::vec3(0.1f, 0.1f, 0.1f));
      terrainPhong->setModel(scaleMatrix);
    }

    _core->addDrawable(terrainPhong, AlphaType::OPAQUE);
  }
  // draw textured Sprite Phong without specular
  {
    auto textureColor = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../sprite/assets/brickwall.jpg"}),
        settings->getLoadTextureAuxilaryFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto textureNormal = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../sprite/assets/brickwall_normal.jpg"}),
        settings->getLoadTextureAuxilaryFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto sprite = std::make_shared<Sprite>(
        std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, lightManager,
        commandBufferTransfer, _core->getResourceManager(), state);
    auto material = std::make_shared<MaterialPhong>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
    material->setBaseColor({textureColor});
    material->setNormal({textureNormal});
    fillMaterialPhong(material);
    sprite->setMaterial(material);
    {
      auto model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 0.f, -3.f));
      model = glm::rotate(model, glm::radians(-90.f), glm::vec3(1.f, 0.f, 0.f));
      model = glm::scale(model, glm::vec3(1.f, 1.f, 1.f));
      sprite->setModel(model);
    }

    _core->addDrawable(sprite);
    _core->addShadowable(sprite);
  }

  {
    auto tile0Color = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/desert/albedo.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile0Metallic = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/desert/metallic.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile0Roughness = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/desert/roughness.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile0AO = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/desert/ao.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);

    auto tile1Color = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/rock/albedo.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile1Metallic = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/rock/metallic.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile1Roughness = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/rock/roughness.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile1AO = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/rock/ao.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile2Color = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/grass/albedo.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile2Metallic = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/grass/metallic.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile2Roughness = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/grass/roughness.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile2AO = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/grass/ao.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile3Color = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/ground/albedo.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile3Metallic = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/ground/metallic.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile3Roughness = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/ground/roughness.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto tile3AO = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/ground/ao.png"}),
        settings->getLoadTextureColorFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);

    auto terrainPBR = std::make_shared<Terrain>(
        _core->getResourceManager()->loadImage({"../../terrain/assets/heightmap.png"}), std::pair{12, 12},
        std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, commandBufferTransfer,
        lightManager, state);
    auto materialPBR = std::make_shared<MaterialPBR>(MaterialTarget::TERRAIN, commandBufferTransfer, state);
    materialPBR->setBaseColor({tile0Color, tile1Color, tile2Color, tile3Color});
    materialPBR->setMetallic({tile0Metallic, tile1Metallic, tile2Metallic, tile3Metallic});
    materialPBR->setRoughness({tile0Roughness, tile1Roughness, tile2Roughness, tile3Roughness});
    materialPBR->setOccluded({tile0AO, tile1AO, tile2AO, tile3AO});
    fillMaterialTerrainPBR(materialPBR);
    terrainPBR->setMaterial(materialPBR);
    {
      auto translateMatrix = glm::translate(glm::mat4(1.f), glm::vec3(3.f, -2.f, 3.f));
      auto scaleMatrix = glm::scale(translateMatrix, glm::vec3(0.01f, 0.01f, 0.01f));
      terrainPBR->setModel(scaleMatrix);
    }

    _core->addDrawable(terrainPBR);
    _core->addShadowable(terrainPBR);
  }

  // draw skeletal textured model with multiple animations
  auto gltfModelFish = _core->getResourceManager()->loadModel("../../model/assets/Fish/scene.gltf");
  {
    auto modelFish = std::make_shared<Model3D>(
        std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, gltfModelFish->getNodes(),
        gltfModelFish->getMeshes(), lightManager, commandBufferTransfer, _core->getResourceManager(), state);
    auto materialModelFish = gltfModelFish->getMaterialsPhong();
    for (auto& material : materialModelFish) {
      fillMaterialPhong(material);
    }
    modelFish->setMaterial(materialModelFish);
    auto animationFish = std::make_shared<Animation>(gltfModelFish->getNodes(), gltfModelFish->getSkins(),
                                                     gltfModelFish->getAnimations(), state);
    animationFish->setAnimation("swim");
    // set animation to model, so joints will be passed to shader
    modelFish->setAnimation(animationFish);
    // register to play animation, if don't call, there will not be any animation,
    // even model can disappear because of zero start weights
    _core->addAnimation(animationFish);
    {
      auto model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 3.f, 3.f));
      model = glm::scale(model, glm::vec3(5.f, 5.f, 5.f));
      modelFish->setModel(model);
    }
    _core->addDrawable(modelFish);
    _core->addShadowable(modelFish);
  }

  // draw textured Sprite with PBR
  {
    auto textureTree = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../sprite/assets/tree.png"}),
        settings->getLoadTextureAuxilaryFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto spriteTree = std::make_shared<Sprite>(
        std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, lightManager,
        commandBufferTransfer, _core->getResourceManager(), state);
    auto materialPBR = std::make_shared<MaterialPBR>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
    materialPBR->setBaseColor({textureTree});
    fillMaterialPBR(materialPBR);
    spriteTree->setMaterial(materialPBR);
    {
      auto model = glm::translate(glm::mat4(1.f), glm::vec3(-3.f, -5.f, -5.f));
      model = glm::scale(model, glm::vec3(1.f, 1.f, 1.f));
      spriteTree->setModel(model);
    }
    _core->addDrawable(spriteTree);
    _core->addShadowable(spriteTree);
  }

  // draw textured Sprite with PBR horizontal
  {
    auto textureTree = std::make_shared<Texture>(
        _core->getResourceManager()->loadImage({"../../sprite/assets/tree.png"}),
        settings->getLoadTextureAuxilaryFormat(), VK_SAMPLER_ADDRESS_MODE_REPEAT, mipMapLevels, commandBufferTransfer,
        state);
    auto spriteTree = std::make_shared<Sprite>(
        std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, lightManager,
        commandBufferTransfer, _core->getResourceManager(), state);
    auto materialPBR = std::make_shared<MaterialPBR>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
    materialPBR->setBaseColor({textureTree});
    fillMaterialPBR(materialPBR);
    spriteTree->setMaterial(materialPBR);
    {
      auto model = glm::translate(glm::mat4(1.f), glm::vec3(3.f, 0.f, -6.f));
      model = glm::rotate(model, glm::radians(-90.f), glm::vec3(1.f, 0.f, 0.f));
      model = glm::scale(model, glm::vec3(1.f, 1.f, 1.f));
      spriteTree->setModel(model);
    }
    _core->addDrawable(spriteTree);
    _core->addShadowable(spriteTree);
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

  angleHorizontal += 0.05f;
  angleVertical += 0.1f;

  auto [FPSLimited, FPSReal] = _core->getFPS();
  auto [widthScreen, heightScreen] = _core->getState()->getSettings()->getResolution();
  _core->getGUI()->startWindow("Help", {20, 20}, {widthScreen / 10, 0});
  _core->getGUI()->drawText({"Limited FPS: " + std::to_string(FPSLimited)});
  _core->getGUI()->drawText({"Maximum FPS: " + std::to_string(FPSReal)});
  if (_core->getGUI()->drawSlider(
          {{"Directional", &_directionalValue},
           {"Point horizontal", &_pointHorizontalValue},
           {"Point vertical", &_pointVerticalValue}},
          {{"Directional", {0.f, 20.f}}, {"Point horizontal", {0.f, 20.f}}, {"Point vertical", {0.f, 20.f}}})) {
    _directionalLight->setColor(glm::vec3(_directionalValue, _directionalValue, _directionalValue));
    _pointLightHorizontal->setColor(glm::vec3(_pointHorizontalValue, _pointHorizontalValue, _pointHorizontalValue));
    _pointLightVertical->setColor(glm::vec3(_pointVerticalValue, _pointVerticalValue, _pointVerticalValue));
  }
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