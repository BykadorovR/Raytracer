#include "DebugVisualization.h"
#include "Descriptor.h"
#include <format>

DebugVisualization::DebugVisualization(std::shared_ptr<Camera> camera, std::shared_ptr<Core> core) {
  _camera = camera;
  _core = core;
  auto state = core->getState();
  auto commandBufferTransfer = _core->getCommandBufferTransfer();
  auto lightManager = core->getLightManager();
  auto resourceManager = core->getResourceManager();

  for (auto elem : _core->getState()->getSettings()->getAttenuations()) {
    _attenuationKeys.push_back(std::to_string(std::get<0>(elem)));
  }

  _lineFrustum.resize(12);
  for (int i = 0; i < _lineFrustum.size(); i++) {
    auto line = std::make_shared<Line>(
        3, std::vector{state->getSettings()->getGraphicColorFormat(), state->getSettings()->getGraphicColorFormat()},
        commandBufferTransfer, state);
    auto mesh = line->getMesh();
    mesh->setColor({glm::vec3(1.f, 0.f, 0.f)}, commandBufferTransfer);
    _lineFrustum[i] = line;
  }

  _near = _camera->getNear();
  _far = _camera->getFar();
  auto [r, g, b, a] = state->getSettings()->getClearColor().float32;
  _R = r;
  _G = g;
  _B = b;

  _farPlaneCW = std::make_shared<Sprite>(
      std::vector{state->getSettings()->getGraphicColorFormat(), state->getSettings()->getGraphicColorFormat()},
      lightManager, commandBufferTransfer, resourceManager, state);
  _farPlaneCW->enableLighting(false);
  _farPlaneCW->enableShadow(false);
  _farPlaneCW->enableDepth(false);
  _farPlaneCCW = std::make_shared<Sprite>(
      std::vector{state->getSettings()->getGraphicColorFormat(), state->getSettings()->getGraphicColorFormat()},
      lightManager, commandBufferTransfer, resourceManager, state);
  _farPlaneCCW->enableLighting(false);
  _farPlaneCCW->enableShadow(false);
  _farPlaneCCW->enableDepth(false);

  auto boxModel = core->getResourceManager()->loadModel("assets/box/Box.gltf");
  for (auto light : lightManager->getPointLights()) {
    _pointValue = light->getColor()[0];
    auto model = std::make_shared<Model3D>(
        std::vector{state->getSettings()->getGraphicColorFormat(), state->getSettings()->getGraphicColorFormat()},
        boxModel->getNodes(), boxModel->getMeshes(), lightManager, commandBufferTransfer, resourceManager, state);
    model->enableDepth(false);
    model->enableShadow(false);
    model->enableLighting(false);
    core->addDrawable(model);
    _pointLightModels.push_back(model);

    auto sphereMesh = std::make_shared<MeshSphere>(commandBufferTransfer, state);
    auto sphere = std::make_shared<Shape3D>(
        ShapeType::SPHERE,
        std::vector{state->getSettings()->getGraphicColorFormat(), state->getSettings()->getGraphicColorFormat()},
        VK_CULL_MODE_NONE, lightManager, commandBufferTransfer, resourceManager, state);
    _spheres.push_back(sphere);
  }

  for (auto light : lightManager->getDirectionalLights()) {
    _directionalValue = light->getColor()[0];
    auto model = std::make_shared<Model3D>(
        std::vector{state->getSettings()->getGraphicColorFormat(), state->getSettings()->getGraphicColorFormat()},
        boxModel->getNodes(), boxModel->getMeshes(), lightManager, commandBufferTransfer, resourceManager, state);
    model->enableDepth(false);
    model->enableShadow(false);
    model->enableLighting(false);
    _core->addDrawable(model);
    _directionalLightModels.push_back(model);
  }

  glm::mat4 model = glm::mat4(1.f);
  auto [resX, resY] = state->getSettings()->getResolution();
  float aspectY = (float)resX / (float)resY;
  glm::vec3 scale = glm::vec3(0.4f, aspectY * 0.4f, 1.f);
  // we don't multiple on view / projection so it's raw coords in NDC space (-1, 1)
  // we shift by x to right and by y to down, that's why -1.f + by Y axis
  // size of object is 1.0f, scale is 0.5f, so shift by X should be 0.25f so right edge is on right edge of screen
  // the same with Y
  model = glm::translate(model, glm::vec3(1.f - (0.5f * scale.x), -1.f + (0.5f * scale.y), 0.f));

  // need to compensate aspect ratio, texture is square but screen resolution is not
  model = glm::scale(model, scale);
  _spriteShadow = std::make_shared<Sprite>(
      std::vector{state->getSettings()->getGraphicColorFormat(), state->getSettings()->getGraphicColorFormat()},
      lightManager, commandBufferTransfer, resourceManager, state);
  _spriteShadow->setModel(model);
  _spriteShadow->enableHUD(true);
  _materialShadow = std::make_shared<MaterialColor>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
  _materialShadow->setBaseColor({core->getResourceManager()->getTextureOne()});
  _spriteShadow->setMaterial(_materialShadow);
  {
    auto camera = std::dynamic_pointer_cast<CameraFly>(_camera);
    auto [resX, resY] = _core->getState()->getSettings()->getResolution();
    float height1 = 2 * tan(glm::radians(camera->getFOV() / 2.f)) * camera->getNear();
    float width1 = height1 * ((float)resX / (float)resY);
    _lineFrustum[0]->getMesh()->setPosition({glm::vec3(-width1 / 2.f, -height1 / 2.f, -camera->getNear()),
                                             glm::vec3(width1 / 2.f, -height1 / 2.f, -camera->getNear())},
                                            commandBufferTransfer);
    _lineFrustum[1]->getMesh()->setPosition({glm::vec3(-width1 / 2.f, height1 / 2.f, -camera->getNear()),
                                             glm::vec3(width1 / 2.f, height1 / 2.f, -camera->getNear())},
                                            commandBufferTransfer);
    _lineFrustum[2]->getMesh()->setPosition({glm::vec3(-width1 / 2.f, -height1 / 2.f, -camera->getNear()),
                                             glm::vec3(-width1 / 2.f, height1 / 2.f, -camera->getNear())},
                                            commandBufferTransfer);
    _lineFrustum[3]->getMesh()->setPosition({glm::vec3(width1 / 2.f, -height1 / 2.f, -camera->getNear()),
                                             glm::vec3(width1 / 2.f, height1 / 2.f, -camera->getNear())},
                                            commandBufferTransfer);
    float height2 = 2 * tan(glm::radians(camera->getFOV() / 2.f)) * camera->getFar();
    float width2 = height2 * ((float)resX / (float)resY);
    _lineFrustum[4]->getMesh()->setPosition({glm::vec3(-width2 / 2.f, -height2 / 2.f, -camera->getFar()),
                                             glm::vec3(width2 / 2.f, -height2 / 2.f, -camera->getFar())},
                                            commandBufferTransfer);
    _lineFrustum[5]->getMesh()->setPosition({glm::vec3(-width2 / 2.f, height2 / 2.f, -camera->getFar()),
                                             glm::vec3(width2 / 2.f, height2 / 2.f, -camera->getFar())},
                                            commandBufferTransfer);
    _lineFrustum[6]->getMesh()->setPosition({glm::vec3(-width2 / 2.f, -height2 / 2.f, -camera->getFar()),
                                             glm::vec3(-width2 / 2.f, height2 / 2.f, -camera->getFar())},
                                            commandBufferTransfer);
    _lineFrustum[7]->getMesh()->setPosition({glm::vec3(width2 / 2.f, -height2 / 2.f, -camera->getFar()),
                                             glm::vec3(width2 / 2.f, height2 / 2.f, -camera->getFar())},
                                            commandBufferTransfer);
    // bottom
    _lineFrustum[8]->getMesh()->setPosition({glm::vec3(0), _lineFrustum[4]->getMesh()->getVertexData()[0].pos},
                                            commandBufferTransfer);
    _lineFrustum[8]->getMesh()->setColor({glm::vec3(0.f, 0.f, 1.f), glm::vec3(0.f, 0.f, 1.f)}, commandBufferTransfer);
    _lineFrustum[9]->getMesh()->setPosition({glm::vec3(0), _lineFrustum[4]->getMesh()->getVertexData()[1].pos},
                                            commandBufferTransfer);
    _lineFrustum[9]->getMesh()->setColor({glm::vec3(0.f, 0.f, 1.f), glm::vec3(0.f, 0.f, 1.f)}, commandBufferTransfer);
    // top
    _lineFrustum[10]->getMesh()->setPosition({glm::vec3(0.f), _lineFrustum[5]->getMesh()->getVertexData()[0].pos},
                                             commandBufferTransfer);
    _lineFrustum[10]->getMesh()->setColor({glm::vec3(0.f, 1.f, 0.f), glm::vec3(0.f, 1.f, 0.f)}, commandBufferTransfer);
    _lineFrustum[11]->getMesh()->setPosition({glm::vec3(0), _lineFrustum[5]->getMesh()->getVertexData()[1].pos},
                                             commandBufferTransfer);
    _lineFrustum[11]->getMesh()->setColor({glm::vec3(0.f, 1.f, 0.f), glm::vec3(0.f, 1.f, 0.f)}, commandBufferTransfer);
  }
}

void DebugVisualization::_drawShadowMaps() {
  auto currentFrame = _core->getState()->getFrameInFlight();
  auto lightManager = _core->getLightManager();
  auto gui = _core->getGUI();
  if (_showDepth) {
    if (_initializedDepth == false) {
      for (int i = 0; i < lightManager->getDirectionalLights().size(); i++) {
        glm::vec3 pos = lightManager->getDirectionalLights()[i]->getPosition();
        _shadowKeys.push_back("Dir " + std::to_string(i) + ": " + std::format("{:.2f}", pos.x) + "x" +
                              std::format("{:.2f}", pos.y));
      }
      for (int i = 0; i < lightManager->getPointLights().size(); i++) {
        for (int j = 0; j < 6; j++) {
          glm::vec3 pos = lightManager->getPointLights()[i]->getPosition();
          _shadowKeys.push_back("Point " + std::to_string(i) + ": " + std::format("{:.2f}", pos.x) + "x" +
                                std::format("{:.2f}", pos.y) + "_" + std::to_string(j));
        }
      }

      _initializedDepth = true;
    }

    if (lightManager->getDirectionalLights().size() + lightManager->getPointLights().size() > 0) {
      std::map<std::string, int*> toggleShadows;
      toggleShadows["##Shadows"] = &_shadowMapIndex;
      gui->drawListBox(_shadowKeys, toggleShadows);

      std::shared_ptr<Texture> currentTexture;
      // check if directional
      std::shared_ptr<DescriptorSet> shadowDescriptorSet;
      if (_shadowMapIndex < lightManager->getDirectionalLights().size()) {
        currentTexture = lightManager->getDirectionalLights()[_shadowMapIndex]->getDepthTexture()[currentFrame];
      } else {
        int pointIndex = _shadowMapIndex - lightManager->getDirectionalLights().size();
        int faceIndex = pointIndex % 6;
        // find index of point light
        pointIndex /= 6;
        currentTexture = lightManager->getPointLights()[pointIndex]
                             ->getDepthCubemap()[currentFrame]
                             ->getTextureSeparate()[faceIndex][0];
      }

      _spriteShadow->setModel(_spriteShadow->getModel());
      _materialShadow->setBaseColor({currentTexture});

      auto drawables = _core->getDrawables(AlphaType::OPAQUE);
      if (std::find(drawables.begin(), drawables.end(), _spriteShadow) == drawables.end()) {
        _core->addDrawable(_spriteShadow, AlphaType::OPAQUE);
      }
    }
  } else {
    auto drawables = _core->getDrawables(AlphaType::OPAQUE);
    if (std::find(drawables.begin(), drawables.end(), _spriteShadow) != drawables.end()) {
      _core->removeDrawable(_spriteShadow);
    }
  }
}

void DebugVisualization::_drawFrustum() {
  if (_frustumDraw) {
    for (auto& line : _lineFrustum) {
      auto drawables = _core->getDrawables(AlphaType::OPAQUE);
      if (std::find(drawables.begin(), drawables.end(), line) == drawables.end()) {
        _core->addDrawable(line, AlphaType::OPAQUE);
      }
    }
  } else {
    for (auto& line : _lineFrustum) {
      auto drawables = _core->getDrawables(AlphaType::OPAQUE);
      if (std::find(drawables.begin(), drawables.end(), line) != drawables.end()) {
        _core->removeDrawable(line);
      }
    }
  }
}

void DebugVisualization::update() {
  auto gui = _core->getGUI();

  auto [resX, resY] = _core->getState()->getSettings()->getResolution();
  auto eye = _camera->getEye();
  auto direction = _camera->getDirection();
  if (gui->startTree("Coordinates")) {
    gui->drawText({std::string("eye x: ") + std::format("{:.2f}", eye.x),
                   std::string("eye y: ") + std::format("{:.2f}", eye.y),
                   std::string("eye z: ") + std::format("{:.2f}", eye.z)});
    gui->drawText({std::string("dir x: ") + std::format("{:.2f}", direction.x),
                   std::string("dir y: ") + std::format("{:.2f}", direction.y),
                   std::string("dir z: ") + std::format("{:.2f}", direction.z)});
    if (gui->drawButton("Save camera")) {
      _eyeSave = _camera->getEye();
      _dirSave = _camera->getDirection();
      _upSave = _camera->getUp();
      _angles = std::dynamic_pointer_cast<CameraFly>(_camera)->getAngles();
    }

    if (gui->drawButton("Load camera")) {
      _camera->setViewParameters(_eyeSave, _dirSave, _upSave);
      std::dynamic_pointer_cast<CameraFly>(_camera)->setAngles(_angles.x, _angles.y, _angles.z);
    }
    gui->endTree();
  }

  if (gui->startTree("Frustum")) {
    std::string buttonText = "Hide frustum";
    if (_frustumDraw == false) {
      buttonText = "Show frustum";
    }

    if (gui->drawInputFloat({{"near", &_near}})) {
      auto cameraFly = std::dynamic_pointer_cast<CameraFly>(_camera);
      cameraFly->setProjectionParameters(cameraFly->getFOV(), _near, _camera->getFar());
    }

    if (gui->drawInputFloat({{"far", &_far}})) {
      auto cameraFly = std::dynamic_pointer_cast<CameraFly>(_camera);
      cameraFly->setProjectionParameters(cameraFly->getFOV(), _camera->getNear(), _far);
    }

    auto clicked = gui->drawButton(buttonText);
    gui->drawCheckbox({{"Show planes", &_showPlanes}});
    if (_frustumDraw && _showPlanes) {
      if (_planesRegistered == false) {
        _core->addDrawable(_farPlaneCW);
        _core->addDrawable(_farPlaneCCW);
        _planesRegistered = true;
      }
    } else {
      if (_planesRegistered == true) {
        _core->removeDrawable(_farPlaneCW);
        _core->removeDrawable(_farPlaneCCW);
        _planesRegistered = false;
      }
    }
    if (clicked) {
      if (_frustumDraw == true) {
        _frustumDraw = false;
      } else {
        _frustumDraw = true;
        auto camera = std::dynamic_pointer_cast<CameraFly>(_camera);
        auto [resX, resY] = _core->getState()->getSettings()->getResolution();
        float height1 = 2 * tan(glm::radians(camera->getFOV() / 2.f)) * camera->getNear();
        float width1 = height1 * ((float)resX / (float)resY);
        float height2 = 2 * tan(glm::radians(camera->getFOV() / 2.f)) * camera->getFar();
        float width2 = height2 * ((float)resX / (float)resY);

        auto model = glm::scale(glm::mat4(1.f), glm::vec3(width2, height2, 1.f));
        model = glm::translate(model, glm::vec3(0.f, 0.f, -camera->getFar()));
        _farPlaneCW->setModel(glm::inverse(camera->getView()) * model);
        model = glm::rotate(model, glm::radians(180.f), glm::vec3(1, 0, 0));
        _farPlaneCCW->setModel(glm::inverse(camera->getView()) * model);
        for (auto& line : _lineFrustum) {
          line->setModel(glm::inverse(camera->getView()));
        }
      }
    }
    _drawFrustum();
    gui->endTree();
  }
  if (gui->startTree("Postprocessing")) {
    float gamma = _core->getPostprocessing()->getGamma();
    if (_core->getGUI()->drawInputFloat({{"gamma", &gamma}})) _core->getPostprocessing()->setGamma(gamma);
    float exposure = _core->getPostprocessing()->getExposure();
    if (_core->getGUI()->drawInputFloat({{"exposure", &exposure}})) _core->getPostprocessing()->setExposure(exposure);
    int blurKernelSize = _core->getBlur()->getKernelSize();
    if (_core->getGUI()->drawInputInt({{"Kernel", &blurKernelSize}})) _core->getBlur()->setKernelSize(blurKernelSize);
    int blurSigma = _core->getBlur()->getSigma();
    if (_core->getGUI()->drawInputInt({{"Sigma", &blurSigma}})) _core->getBlur()->setSigma(blurSigma);
    int bloomPasses = _core->getState()->getSettings()->getBloomPasses();
    if (_core->getGUI()->drawInputInt({{"Passes", &bloomPasses}}))
      _core->getState()->getSettings()->setBloomPasses(bloomPasses);

    gui->drawInputFloat({{"R", &_R}});
    _R = std::min(_R, 1.f);
    _R = std::max(_R, 0.f);
    gui->drawInputFloat({{"G", &_G}});
    _G = std::min(_G, 1.f);
    _G = std::max(_G, 0.f);
    gui->drawInputFloat({{"B", &_B}});
    _B = std::min(_B, 1.f);
    _B = std::max(_B, 0.f);
    _core->getState()->getSettings()->setClearColor({_R, _G, _B, 1.f});
    gui->endTree();
  }

  if (gui->startTree("Light")) {
    if (_core->getGUI()->drawSlider({{"Directional", &_directionalValue}, {"Point", &_pointValue}},
                                    {{"Directional", {0.f, 20.f}}, {"Point", {0.f, 20.f}}})) {
      for (auto& light : _core->getLightManager()->getDirectionalLights()) {
        light->setColor(glm::vec3(_directionalValue, _directionalValue, _directionalValue));
      }
      for (auto& light : _core->getLightManager()->getPointLights()) {
        light->setColor(glm::vec3(_pointValue, _pointValue, _pointValue));
      }
    }

    std::map<std::string, bool*> toggle;
    toggle["Lights"] = &_showLights;
    gui->drawCheckbox(toggle);
    if (_showLights) {
      std::map<std::string, bool*> toggle;
      toggle["Spheres"] = &_enableSpheres;
      gui->drawCheckbox(toggle);
      if (_enableSpheres) {
        std::map<std::string, int*> toggleSpheres;
        toggleSpheres["##Spheres"] = &_lightSpheresIndex;
        gui->drawListBox(_attenuationKeys, toggleSpheres);
      }
      for (int i = 0; i < _core->getLightManager()->getPointLights().size(); i++) {
        if (_registerLights) _core->addDrawable(_pointLightModels[i]);
        {
          auto model = glm::translate(glm::mat4(1.f), _core->getLightManager()->getPointLights()[i]->getPosition());
          model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f));
          _pointLightModels[i]->setModel(model);
        }
        {
          auto model = glm::translate(glm::mat4(1.f), _core->getLightManager()->getPointLights()[i]->getPosition());
          if (_enableSpheres) {
            if (_lightSpheresIndex < 0)
              _lightSpheresIndex = _core->getLightManager()->getPointLights()[i]->getAttenuationIndex();
            _core->getLightManager()->getPointLights()[i]->setAttenuationIndex(_lightSpheresIndex);
            int distance = _core->getLightManager()->getPointLights()[i]->getDistance();
            model = glm::scale(model, glm::vec3(distance, distance, distance));
            _spheres[i]->setModel(model);
            _spheres[i]->setDrawType(DrawType::WIREFRAME);
            auto drawables = _core->getDrawables(AlphaType::OPAQUE);
            if (std::find(drawables.begin(), drawables.end(), _spheres[i]) == drawables.end()) {
              _core->addDrawable(_spheres[i], AlphaType::OPAQUE);
            }
          } else {
            auto drawables = _core->getDrawables(AlphaType::OPAQUE);
            if (std::find(drawables.begin(), drawables.end(), _spheres[i]) != drawables.end()) {
              _core->removeDrawable(_spheres[i]);
            }
          }
        }
      }
      for (int i = 0; i < _core->getLightManager()->getDirectionalLights().size(); i++) {
        if (_registerLights) _core->addDrawable(_directionalLightModels[i]);
        auto model = glm::translate(glm::mat4(1.f), _core->getLightManager()->getDirectionalLights()[i]->getPosition());
        model = glm::scale(model, glm::vec3(0.2f, 0.2f, 0.2f));
        _directionalLightModels[i]->setModel(model);
      }
      _registerLights = false;
    } else {
      if (_registerLights == false) {
        _registerLights = true;
        for (int i = 0; i < _core->getLightManager()->getPointLights().size(); i++) {
          _core->removeDrawable(_pointLightModels[i]);
        }
        for (int i = 0; i < _core->getLightManager()->getDirectionalLights().size(); i++) {
          _core->removeDrawable(_directionalLightModels[i]);
        }
      }
    }
    std::map<std::string, bool*> toggleDepth;
    toggleDepth["Depth"] = &_showDepth;
    gui->drawCheckbox(toggleDepth);
    _drawShadowMaps();

    gui->endTree();
  }
}

void DebugVisualization::cursorNotify(GLFWwindow* window, float xPos, float yPos) {}

void DebugVisualization::mouseNotify(GLFWwindow* window, int button, int action, int mods) {}

void DebugVisualization::keyNotify(GLFWwindow* window, int key, int scancode, int action, int mods) {
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

void DebugVisualization::charNotify(GLFWwindow* window, unsigned int code) {}

void DebugVisualization::scrollNotify(GLFWwindow* window, double xOffset, double yOffset) {}