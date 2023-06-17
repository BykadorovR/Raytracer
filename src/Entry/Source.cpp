#include <iostream>
#include <chrono>
#include <future>

#include "Window.h"
#include "Instance.h"
#include "Surface.h"
#include "Device.h"
#include "Swapchain.h"
#include "Render.h"
#include "Buffer.h"
#include "Shader.h"
#include "Pipeline.h"
#include "Command.h"
#include "Sync.h"
#include "Command.h"
#include "Settings.h"
#include "Sprite.h"
#include "SpriteManager.h"
#include "Model.h"
#include "ModelManager.h"
#include "GUI.h"
#include "Input.h"
#include "Camera.h"
#include "LightManager.h"
#include "DebugVisualization.h"
#include "State.h"
#include "Logger.h"

#undef near
#undef far

float fps = 0;
float frameTimer = 0.f;
uint64_t currentFrame = 0;
// Depth bias (and slope) are used to avoid shadowing artifacts
// Constant depth bias factor (always applied)
float depthBiasConstant = 1.25f;
// Slope depth bias factor, applied depending on polygon's slope
float depthBiasSlope = 1.75f;

std::shared_ptr<Window> window;
std::shared_ptr<Instance> instance;
std::shared_ptr<Device> device;
std::shared_ptr<CommandBuffer> commandBuffer, commandBufferTransfer;
std::shared_ptr<CommandPool> commandPool, commandPoolTransfer;
std::shared_ptr<DescriptorPool> descriptorPool;
std::shared_ptr<Surface> surface;
std::shared_ptr<Settings> settings;
std::shared_ptr<State> state;
std::array<VkClearValue, 2> clearValues{};

std::vector<std::shared_ptr<Semaphore>> imageAvailableSemaphores, renderFinishedSemaphores;
std::vector<std::shared_ptr<Fence>> inFlightFences;

std::shared_ptr<SpriteManager> spriteManager;
std::shared_ptr<Model3DManager> modelManager;
std::shared_ptr<ModelGLTF> modelGLTF;
std::shared_ptr<Input> input;
std::shared_ptr<GUI> gui;
std::shared_ptr<Swapchain> swapchain;
std::shared_ptr<RenderPass> renderPass, renderPassDepth;
std::shared_ptr<Framebuffer> frameBuffer;
std::vector<std::vector<std::shared_ptr<Framebuffer>>> frameBufferDepth;
std::shared_ptr<CameraFly> camera;
std::shared_ptr<LightManager> lightManager;
std::shared_ptr<PointLight> pointLightHorizontal, pointLightVertical;
std::shared_ptr<DirectionalLight> directionalLight, directionalLight2;
std::shared_ptr<DebugVisualization> debugVisualization;
std::shared_ptr<LoggerGPU> loggerGPU;
std::shared_ptr<LoggerCPU> loggerCPU;

std::tuple<int, int> depthResolution = {256, 256};

void initialize() {
  state = std::make_shared<State>(
      std::make_shared<Settings>("Vulkan", std::tuple{1600, 900}, VK_FORMAT_B8G8R8A8_UNORM, 2));
  settings = state->getSettings();
  window = state->getWindow();
  input = state->getInput();
  instance = state->getInstance();
  surface = state->getSurface();
  device = state->getDevice();
  swapchain = state->getSwapchain();
  descriptorPool = state->getDescriptorPool();
  // allocate commands
  commandPool = std::make_shared<CommandPool>(QueueType::GRAPHIC, device);
  commandBuffer = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(), commandPool, device);
  commandPoolTransfer = std::make_shared<CommandPool>(QueueType::TRANSFER, device);
  commandBufferTransfer = std::make_shared<CommandBuffer>(1, commandPool, device);
  //
  loggerGPU = std::make_shared<LoggerGPU>(commandBuffer, state);
  loggerCPU = std::make_shared<LoggerCPU>();

  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    imageAvailableSemaphores.push_back(std::make_shared<Semaphore>(device));
    renderFinishedSemaphores.push_back(std::make_shared<Semaphore>(device));
    inFlightFences.push_back(std::make_shared<Fence>(device));
  }

  renderPass = std::make_shared<RenderPass>(device);
  renderPass->initialize(swapchain->getImageFormat());
  renderPassDepth = std::make_shared<RenderPass>(device);
  renderPassDepth->initializeDepthPass();

  gui = std::make_shared<GUI>(settings->getResolution(), window, device);
  gui->initialize(renderPass, commandBufferTransfer);

  auto texture = std::make_shared<Texture>("../data/brickwall.jpg", VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                           commandBufferTransfer, device);
  auto normalMap = std::make_shared<Texture>("../data/brickwall_normal.jpg", VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                             commandBufferTransfer, device);
  camera = std::make_shared<CameraFly>(settings);
  input->subscribe(std::dynamic_pointer_cast<InputSubscriber>(camera));
  input->subscribe(std::dynamic_pointer_cast<InputSubscriber>(gui));
  lightManager = std::make_shared<LightManager>(commandBufferTransfer, state);
  pointLightHorizontal = lightManager->createPointLight(depthResolution);
  pointLightHorizontal->createPhong(0.f, 1.f, glm::vec3(1.f, 1.f, 1.f));
  pointLightHorizontal->setAttenuation(1.f, 0.09f, 0.032f);
  pointLightHorizontal->setPosition({3.f, 4.f, 0.f});

  pointLightVertical = lightManager->createPointLight(depthResolution);
  pointLightVertical->createPhong(0.f, 1.f, glm::vec3(1.f, 1.f, 1.f));
  pointLightVertical->setAttenuation(1.f, 0.09f, 0.032f);
  pointLightVertical->setPosition({-3.f, 4.f, 0.f});

  directionalLight = lightManager->createDirectionalLight(depthResolution);
  directionalLight->createPhong(0.f, 1.f, glm::vec3(1.0f, 1.0f, 1.0f));
  directionalLight->setPosition({0.f, 15.f, 0.f});
  directionalLight->setCenter({0.f, 0.f, 0.f});
  directionalLight->setUp({0.f, 0.f, 1.f});

  /*directionalLight2 = lightManager->createDirectionalLight();
  directionalLight2->createPhong(0.f, 1.f, glm::vec3(1.f, 1.f, 1.f));
  directionalLight2->setPosition({15.f, 3.f, 0.f});
  directionalLight2->setCenter({0.f, 0.f, 0.f});
  directionalLight2->setUp({0.f, 1.f, 0.f});*/

  spriteManager = std::make_shared<SpriteManager>(lightManager, commandBuffer, commandBufferTransfer, descriptorPool,
                                                  renderPass, renderPassDepth, device, settings);
  modelManager = std::make_shared<Model3DManager>(lightManager, commandBuffer, commandBufferTransfer, descriptorPool,
                                                  renderPass, renderPassDepth, device, settings);
  debugVisualization = std::make_shared<DebugVisualization>(camera, gui, commandBuffer, commandBufferTransfer, state);
  debugVisualization->setLights(modelManager, lightManager);
  if (directionalLight) debugVisualization->setTexture(directionalLight->getDepthTexture()[0]);
  input->subscribe(std::dynamic_pointer_cast<InputSubscriber>(debugVisualization));
  {
    auto sprite = spriteManager->createSprite(texture, normalMap);
    auto sprite2 = spriteManager->createSprite(texture, normalMap);
    auto sprite3 = spriteManager->createSprite(texture, normalMap);
    auto sprite4 = spriteManager->createSprite(texture, normalMap);
    auto sprite5 = spriteManager->createSprite(texture, normalMap);
    auto sprite6 = spriteManager->createSprite(texture, normalMap);
    {
      glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, 1.f));
      model = glm::rotate(model, glm::radians(180.f), glm::vec3(0.f, 1.f, 0.f));
      sprite6->setModel(model);
    }
    {
      glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(-0.5f, 0.f, 0.5f));
      model = glm::rotate(model, glm::radians(90.f), glm::vec3(0.f, 1.f, 0.f));
      sprite2->setModel(model);
    }
    {
      glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(0.5f, 0.f, 0.5f));
      model = glm::rotate(model, glm::radians(-90.f), glm::vec3(0.f, 1.f, 0.f));
      sprite3->setModel(model);
    }
    {
      glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(0.0f, 0.5f, 0.5f));
      model = glm::rotate(model, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
      sprite4->setModel(model);
    }
    {
      glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(0.0f, -0.5f, 0.5f));
      model = glm::rotate(model, glm::radians(-90.f), glm::vec3(1.f, 0.f, 0.f));
      sprite5->setModel(model);
    }

    spriteManager->registerSprite(sprite);
    spriteManager->registerSprite(sprite2);
    spriteManager->registerSprite(sprite3);
    spriteManager->registerSprite(sprite4);
    spriteManager->registerSprite(sprite5);
    spriteManager->registerSprite(sprite6);
  }
  {
    auto sprite = spriteManager->createSprite(texture, normalMap);
    {
      glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -3.f, 0.f));
      model = glm::scale(model, glm::vec3(15.f, 15.f, 15.f));
      model = glm::rotate(model, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
      sprite->setModel(model);
    }

    spriteManager->registerSprite(sprite);
  }
  // modelGLTF = modelManager->createModelGLTF("../data/Avocado/Avocado.gltf");
  // modelGLTF = modelManager->createModelGLTF("../data/CesiumMan/CesiumMan.gltf");
  modelGLTF = modelManager->createModelGLTF("../data/BrainStem/BrainStem.gltf");
  // modelGLTF = modelManager->createModelGLTF("../data/SimpleSkin/SimpleSkin.gltf");
  // modelGLTF = modelManager->createModelGLTF("../data/Sponza/Sponza.gltf");
  // modelGLTF = modelManager->createModelGLTF("../data/DamagedHelmet/DamagedHelmet.gltf");
  // modelGLTF = modelManager->createModelGLTF("../data/Box/BoxTextured.gltf");
  //{
  //   glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(-2.f, -1.f, 0.f));
  //   // model = glm::rotate(model, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
  //   model3D->setModel(model);
  // }
  {
    glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -3.f));
    // model = glm::rotate(model, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
    // model = glm::scale(model, glm::vec3(20.f, 20.f, 20.f));
    modelGLTF->setModel(model);
  }

  modelManager->registerModelGLTF(modelGLTF);

  frameBuffer = std::make_shared<Framebuffer>(swapchain->getImageViews(), swapchain->getDepthImageView(), renderPass,
                                              device);

  int pointNumber = lightManager->getPointLights().size();
  int directionalNumber = lightManager->getDirectionalLights().size();
  frameBufferDepth.resize(pointNumber + directionalNumber);
  for (int i = 0; i < directionalNumber; i++) {
    auto textures = lightManager->getDirectionalLights()[i]->getDepthTexture();
    std::vector<std::shared_ptr<ImageView>> imageViews(settings->getMaxFramesInFlight());
    for (int j = 0; j < imageViews.size(); j++) imageViews[j] = textures[j]->getImageView();
    frameBufferDepth[i] = {std::make_shared<Framebuffer>(imageViews, renderPassDepth, device)};
  }

  for (int i = 0; i < pointNumber; i++) {
    auto cubemap = lightManager->getPointLights()[i]->getDepthCubemap();
    std::vector<std::pair<std::shared_ptr<ImageView>, std::shared_ptr<ImageView>>> imageViews(6);
    frameBufferDepth[i + directionalNumber].resize(imageViews.size());
    for (int j = 0; j < imageViews.size(); j++) {
      imageViews[j] = {cubemap[0]->getTextureSeparate()[j]->getImageView(),
                       cubemap[1]->getTextureSeparate()[j]->getImageView()};

      auto [width, height] = state->getSettings()->getResolution();
      frameBufferDepth[i + directionalNumber][j] = std::make_shared<Framebuffer>(
          std::vector{imageViews[j].first, imageViews[j].second}, renderPassDepth, device);
    }
  }
}

VkRenderPassBeginInfo render(int index,
                             std::shared_ptr<RenderPass> renderPass,
                             std::shared_ptr<Framebuffer> framebuffer) {
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = renderPass->getRenderPass();
  renderPassInfo.framebuffer = framebuffer->getBuffer()[index];
  renderPassInfo.renderArea.offset = {0, 0};
  auto [width, height] = framebuffer->getResolution();
  renderPassInfo.renderArea.extent.width = width;
  renderPassInfo.renderArea.extent.height = height;

  return renderPassInfo;
}

std::future<void> updateJoints;
void drawFrame() {
  auto result = vkWaitForFences(device->getLogicalDevice(), 1, &inFlightFences[currentFrame]->getFence(), VK_TRUE,
                                UINT64_MAX);
  if (result != VK_SUCCESS) throw std::runtime_error("Can't wait for fence");

  uint32_t imageIndex;
  // RETURNS ONLY INDEX, NOT IMAGE
  result = vkAcquireNextImageKHR(device->getLogicalDevice(), swapchain->getSwapchain(), UINT64_MAX,
                                 imageAvailableSemaphores[currentFrame]->getSemaphore(), VK_NULL_HANDLE, &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    // TODO: recreate swapchain
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  result = vkResetFences(device->getLogicalDevice(), 1, &inFlightFences[currentFrame]->getFence());
  if (result != VK_SUCCESS) throw std::runtime_error("Can't reset fence");

  result = vkResetCommandBuffer(commandBuffer->getCommandBuffer()[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
  if (result != VK_SUCCESS) throw std::runtime_error("Can't reset cmd buffer");

  gui->drawText("FPS", {20, 20}, {100, 60}, {std::to_string(fps)});

  // record command buffer
  commandBuffer->beginCommands(currentFrame);

  loggerGPU->initialize("Common", currentFrame);
  // update positions
  static float angleHorizontal = 90.f;
  glm::vec3 lightPositionHorizontal = glm::vec3(15.f * cos(glm::radians(angleHorizontal)), 0.f,
                                                15.f * sin(glm::radians(angleHorizontal)));
  static float angleVertical = 0.f;
  glm::vec3 lightPositionVertical = glm::vec3(0.f, 15.f * sin(glm::radians(angleVertical)),
                                              15.f * cos(glm::radians(angleVertical)));

  // if (directionalLight) directionalLight->setPosition(lightPositionVertical);
  // if (directionalLight2) directionalLight2->setPosition(lightPositionHorizontal);
  angleVertical += 0.01f;
  angleHorizontal += 0.01f;
  spriteManager->setCamera(camera);
  modelManager->setCamera(camera);

  /////////////////////////////////////////////////////////////////////////////////////////
  // render to depth buffer
  /////////////////////////////////////////////////////////////////////////////////////////
  {
    loggerGPU->begin("Directional to depth buffer", currentFrame);
    auto directionalLights = lightManager->getDirectionalLights();
    for (int i = 0; i < directionalLights.size(); i++) {
      auto renderPassInfo = render(currentFrame, renderPassDepth, frameBufferDepth[i][0]);
      clearValues[0].depthStencil = {1.0f, 0};
      renderPassInfo.clearValueCount = 1;
      renderPassInfo.pClearValues = clearValues.data();
      // TODO: only one depth texture?
      vkCmdBeginRenderPass(commandBuffer->getCommandBuffer()[currentFrame], &renderPassInfo,
                           VK_SUBPASS_CONTENTS_INLINE);
      // Set depth bias (aka "Polygon offset")
      // Required to avoid shadow mapping artifacts
      vkCmdSetDepthBias(commandBuffer->getCommandBuffer()[currentFrame], depthBiasConstant, 0.0f, depthBiasSlope);

      // draw scene here
      loggerGPU->begin("Sprites to directional depth buffer", currentFrame);
      spriteManager->drawShadow(currentFrame, LightType::DIRECTIONAL, i);
      loggerGPU->end(currentFrame);

      if (updateJoints.valid()) {
        updateJoints.get();
      }

      loggerGPU->begin("Models to directional depth buffer", currentFrame);
      modelManager->drawShadow(currentFrame, LightType::DIRECTIONAL, i);
      loggerGPU->end(currentFrame);
      vkCmdEndRenderPass(commandBuffer->getCommandBuffer()[currentFrame]);
    }
    loggerGPU->end(currentFrame);

    loggerGPU->begin("Point to depth buffer", currentFrame);
    auto pointLights = lightManager->getPointLights();
    for (int i = 0; i < pointLights.size(); i++) {
      for (int j = 0; j < 6; j++) {
        auto renderPassInfo = render(currentFrame, renderPassDepth, frameBufferDepth[i + directionalLights.size()][j]);
        clearValues[0].depthStencil = {1.0f, 0};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = clearValues.data();

        // TODO: only one depth texture?
        vkCmdBeginRenderPass(commandBuffer->getCommandBuffer()[currentFrame], &renderPassInfo,
                             VK_SUBPASS_CONTENTS_INLINE);
        // Set depth bias (aka "Polygon offset")
        // Required to avoid shadow mapping artifacts
        vkCmdSetDepthBias(commandBuffer->getCommandBuffer()[currentFrame], depthBiasConstant, 0.0f, depthBiasSlope);

        // draw scene here
        float aspect = std::get<0>(settings->getResolution()) / std::get<1>(settings->getResolution());
        loggerGPU->begin("Sprites to point depth buffer", currentFrame);
        spriteManager->drawShadow(currentFrame, LightType::POINT, i, j);
        loggerGPU->end(currentFrame);
        loggerGPU->begin("Models to point depth buffer", currentFrame);
        modelManager->drawShadow(currentFrame, LightType::POINT, i, j);
        loggerGPU->end(currentFrame);
        vkCmdEndRenderPass(commandBuffer->getCommandBuffer()[currentFrame]);
      }
    }
    loggerGPU->end(currentFrame);
  }

  /////////////////////////////////////////////////////////////////////////////////////////
  // depth to screne barrier
  /////////////////////////////////////////////////////////////////////////////////////////
  // Image memory barrier to make sure that writes are finished before sampling from the texture
  int directionalNum = lightManager->getDirectionalLights().size();
  int pointNum = lightManager->getPointLights().size();
  std::vector<VkImageMemoryBarrier> imageMemoryBarrier(directionalNum + pointNum * 6);
  for (int i = 0; i < directionalNum; i++) {
    imageMemoryBarrier[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // We won't be changing the layout of the image
    imageMemoryBarrier[i].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    imageMemoryBarrier[i].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    imageMemoryBarrier[i].image = lightManager->getDirectionalLights()[i]
                                      ->getDepthTexture()[currentFrame]
                                      ->getImageView()
                                      ->getImage()
                                      ->getImage();
    imageMemoryBarrier[i].subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    imageMemoryBarrier[i].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    imageMemoryBarrier[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imageMemoryBarrier[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  }

  for (int i = 0; i < pointNum; i++) {
    for (int j = 0; j < 6; j++) {
      int id = directionalNum + 6 * i + j;
      imageMemoryBarrier[id].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      // We won't be changing the layout of the image
      imageMemoryBarrier[id].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
      imageMemoryBarrier[id].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
      imageMemoryBarrier[id].image = lightManager->getPointLights()[i]
                                         ->getDepthCubemap()[currentFrame]
                                         ->getTextureSeparate()[j]
                                         ->getImageView()
                                         ->getImage()
                                         ->getImage();
      imageMemoryBarrier[id].subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
      imageMemoryBarrier[id].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      imageMemoryBarrier[id].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      imageMemoryBarrier[id].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      imageMemoryBarrier[id].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    }
  }
  // TODO: create 6 memory barriers and send them via one cvCmdPipelineBarrier
  vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, imageMemoryBarrier.size(),
                       imageMemoryBarrier.data());
  /////////////////////////////////////////////////////////////////////////////////////////
  // render to screen
  /////////////////////////////////////////////////////////////////////////////////////////
  {
    loggerGPU->begin("Render to screen", currentFrame);

    auto renderPassInfo = render(imageIndex, renderPass, frameBuffer);
    clearValues[0].color = {0.25f, 0.25f, 0.25f, 1.f};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer->getCommandBuffer()[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    lightManager->draw(currentFrame);
    // draw scene here
    loggerGPU->begin("Render sprites", currentFrame);
    spriteManager->draw(currentFrame);
    loggerGPU->end(currentFrame);

    loggerGPU->begin("Render models", currentFrame);
    modelManager->draw(currentFrame);
    loggerGPU->end(currentFrame);

    updateJoints = std::async([&]() {
      loggerCPU->begin("Update animation");
      modelManager->updateAnimation(frameTimer);
      loggerCPU->end();
    });

    loggerGPU->begin("Render debug visualization", currentFrame);
    debugVisualization->draw(currentFrame);
    loggerGPU->end(currentFrame);

    loggerGPU->begin("Render GUI", currentFrame);
    gui->updateBuffers(currentFrame);
    gui->drawFrame(currentFrame, commandBuffer->getCommandBuffer()[currentFrame]);
    loggerGPU->end(currentFrame);

    vkCmdEndRenderPass(commandBuffer->getCommandBuffer()[currentFrame]);

    loggerGPU->end(currentFrame);
  }

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]->getSemaphore()};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer->getCommandBuffer()[currentFrame];

  VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]->getSemaphore()};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  // end command buffer
  commandBuffer->endCommands(currentFrame, submitInfo, inFlightFences[currentFrame]);

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapChains[] = {swapchain->getSwapchain()};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;

  presentInfo.pImageIndices = &imageIndex;

  // TODO: change to own present queue
  result = vkQueuePresentKHR(device->getQueue(QueueType::PRESENT), &presentInfo);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    // TODO: recreate swapchain
    // TODO: support window resize
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }

  currentFrame = (currentFrame + 1) % settings->getMaxFramesInFlight();
}

void mainLoop() {
  auto startTime = std::chrono::high_resolution_clock::now();
  int frameFPS = 0;
  while (!glfwWindowShouldClose(window->getWindow())) {
    auto startTimeCurrent = std::chrono::high_resolution_clock::now();
    glfwPollEvents();
    drawFrame();
    frameFPS++;
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsedCurrent = end - startTimeCurrent;
    frameTimer = elapsedCurrent.count();
    std::chrono::duration<double> elapsed = end - startTime;
    // calculate frames per second
    if (elapsed.count() > 1.f) {
      fps = frameFPS;
      frameFPS = 0;
      startTime = std::chrono::high_resolution_clock::now();
    }
  }

  vkDeviceWaitIdle(device->getLogicalDevice());
}

int main() {
  try {
    initialize();
    mainLoop();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}