#include <iostream>
#include <chrono>

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
#include "Queue.h"
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
std::shared_ptr<CommandBuffer> commandBuffer;
std::shared_ptr<CommandPool> commandPool;
std::shared_ptr<DescriptorPool> descriptorPool;
std::shared_ptr<Queue> queue;
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
std::shared_ptr<Framebuffer> frameBuffer, frameBufferDepth;
std::vector<std::shared_ptr<ImageView>> resultDepth;
std::shared_ptr<CameraFly> camera;
std::shared_ptr<LightManager> lightManager;
std::shared_ptr<PointLight> pointLightHorizontal, pointLightVertical;
std::shared_ptr<DirectionalLight> directionalLight, directionalLight2;
std::shared_ptr<DebugVisualization> debugVisualization;
std::vector<std::shared_ptr<Texture>> depthTexture;
std::shared_ptr<Logger> logger;

void initialize() {
  state = std::make_shared<State>(
      std::make_shared<Settings>("Vulkan", std::tuple{1600, 900}, VK_FORMAT_B8G8R8A8_UNORM, 2));
  settings = state->getSettings();
  window = state->getWindow();
  input = state->getInput();
  instance = state->getInstance();
  surface = state->getSurface();
  device = state->getDevice();
  commandPool = state->getCommandPool();
  queue = state->getQueue();
  commandBuffer = state->getCommandBuffer();
  swapchain = state->getSwapchain();
  descriptorPool = state->getDescriptorPool();
  logger = std::make_shared<Logger>(state);

  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    imageAvailableSemaphores.push_back(std::make_shared<Semaphore>(device));
    renderFinishedSemaphores.push_back(std::make_shared<Semaphore>(device));
    inFlightFences.push_back(std::make_shared<Fence>(device));
  }

  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    std::shared_ptr<Image> image = std::make_shared<Image>(
        settings->getResolution(), VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        device);
    image->overrideLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    resultDepth.push_back(std::make_shared<ImageView>(image, VK_IMAGE_ASPECT_DEPTH_BIT, device));
    depthTexture.push_back(std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, resultDepth[i], device));
  }

  renderPass = std::make_shared<RenderPass>(device);
  renderPass->initialize(swapchain->getImageFormat());
  renderPassDepth = std::make_shared<RenderPass>(device);
  renderPassDepth->initializeDepthPass();

  frameBuffer = std::make_shared<Framebuffer>(settings->getResolution(), swapchain->getImageViews(),
                                              swapchain->getDepthImageView(), renderPass, device);

  frameBufferDepth = std::make_shared<Framebuffer>(settings->getResolution(), resultDepth, renderPassDepth, device);

  gui = std::make_shared<GUI>(settings->getResolution(), window, device);
  gui->initialize(renderPass, queue, commandPool);

  auto texture = std::make_shared<Texture>("../data/brickwall.jpg", VK_SAMPLER_ADDRESS_MODE_REPEAT, commandPool, queue,
                                           device);
  auto normalMap = std::make_shared<Texture>("../data/brickwall_normal.jpg", VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                             commandPool, queue, device);
  camera = std::make_shared<CameraFly>(settings);
  input->subscribe(std::dynamic_pointer_cast<InputSubscriber>(camera));
  input->subscribe(std::dynamic_pointer_cast<InputSubscriber>(gui));
  lightManager = std::make_shared<LightManager>(settings, device);
  // pointLightHorizontal = lightManager->createPointLight();
  // pointLightHorizontal->createPhong(0.f, 0.5f, glm::vec3(1.f, 1.f, 1.f));
  // pointLightHorizontal->setAttenuation(1.f, 0.09f, 0.032f);

  pointLightVertical = lightManager->createPointLight();
  pointLightVertical->createPhong(0.f, 1.f, glm::vec3(1.f, 1.f, 1.f));
  pointLightVertical->setPosition({0.f, 10.f, 0.f});

  // directionalLight = lightManager->createDirectionalLight();
  // directionalLight->createPhong(0.f, 1.f, glm::vec3(1.f, 1.f, 1.f));
  // directionalLight->setDirection(glm::vec3(-0.2f, -1.f, -0.3f));

  // directionalLight2 = lightManager->createDirectionalLight();
  // directionalLight2->createPhong(0.f, 1.f, glm::vec3(1.f, 0.f, 0.f));
  // directionalLight2->setDirection(glm::vec3(0.f, 1.f, 0.f));

  spriteManager = std::make_shared<SpriteManager>(lightManager, commandPool, commandBuffer, queue, descriptorPool,
                                                  renderPass, renderPassDepth, device, settings);
  modelManager = std::make_shared<Model3DManager>(lightManager, commandPool, commandBuffer, queue, descriptorPool,
                                                  renderPass, renderPassDepth, device, settings);
  debugVisualization = std::make_shared<DebugVisualization>(camera, gui, state, logger);
  debugVisualization->setLights(modelManager, lightManager);
  debugVisualization->setTexture(depthTexture[0]);
  input->subscribe(std::dynamic_pointer_cast<InputSubscriber>(debugVisualization));
  {
    auto sprite = spriteManager->createSprite(texture, normalMap, depthTexture[0]);
    auto sprite2 = spriteManager->createSprite(texture, normalMap, depthTexture[0]);
    auto sprite3 = spriteManager->createSprite(texture, normalMap, depthTexture[0]);
    auto sprite4 = spriteManager->createSprite(texture, normalMap, depthTexture[0]);
    auto sprite5 = spriteManager->createSprite(texture, normalMap, depthTexture[0]);
    auto sprite6 = spriteManager->createSprite(texture, normalMap, depthTexture[0]);
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
    auto sprite = spriteManager->createSprite(texture, normalMap, depthTexture[0]);
    {
      glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -25.f, 0.f));
      model = glm::scale(model, glm::vec3(15.f, 15.f, 15.f));
      model = glm::rotate(model, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
      sprite->setModel(model);
    }

    spriteManager->registerSprite(sprite);
  }
  // modelGLTF = modelManager->createModelGLTF("../data/Avocado/Avocado.gltf", depthTexture[0]);
  // modelGLTF = modelManager->createModelGLTF("../data/CesiumMan/CesiumMan.gltf");
  // modelGLTF = modelManager->createModelGLTF("../data/BrainStem/BrainStem.gltf");
  // modelGLTF = modelManager->createModelGLTF("../data/SimpleSkin/SimpleSkin.gltf");
  modelGLTF = modelManager->createModelGLTF("../data/Sponza/Sponza.gltf", depthTexture[0]);
  // modelGLTF = modelManager->createModelGLTF("../data/DamagedHelmet/DamagedHelmet.gltf");
  // modelGLTF = modelManager->createModelGLTF("../data/Box/BoxTextured.gltf", depthTexture[0]);
  //{
  //  glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(-2.f, -1.f, 0.f));
  //  // model = glm::rotate(model, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
  //  model3D->setModel(model);
  //}
  {
    glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -5.f, 0.f));

    // model = glm::rotate(model, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
    // model = glm::scale(model, glm::vec3(400.f, 400.f, 400.f));
    modelGLTF->setModel(model);
  }

  modelManager->registerModelGLTF(modelGLTF);
}

VkRenderPassBeginInfo render(int index,
                             std::shared_ptr<RenderPass> renderPass,
                             std::shared_ptr<Framebuffer> framebuffer) {
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = renderPass->getRenderPass();
  renderPassInfo.framebuffer = framebuffer->getBuffer()[index];
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent.width = std::get<0>(settings->getResolution());
  renderPassInfo.renderArea.extent.height = std::get<1>(settings->getResolution());

  return renderPassInfo;
}

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
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if (vkBeginCommandBuffer(commandBuffer->getCommandBuffer()[currentFrame], &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }

  logger->setDebugUtils("Common command buffer", currentFrame);
  // update positions
  static float angleHorizontal = 90.f;
  glm::vec3 lightPositionHorizontal = glm::vec3(3.f * cos(glm::radians(angleHorizontal)), 0.f,
                                                3.f * sin(glm::radians(angleHorizontal)));
  static float angleVertical = 0.f;
  glm::vec3 lightPositionVertical = glm::vec3(0.f, 3.f * sin(glm::radians(angleVertical)),
                                              3.f * cos(glm::radians(angleVertical)));

  // if (pointLightHorizontal) pointLightHorizontal->setPosition(lightPositionHorizontal);
  // if (pointLightVertical) pointLightVertical->setPosition(lightPositionVertical);
  angleVertical += 0.01f;
  angleHorizontal += 0.01f;
  spriteManager->setCamera(camera);
  modelManager->setCamera(camera);

  /////////////////////////////////////////////////////////////////////////////////////////
  // render to depth buffer
  /////////////////////////////////////////////////////////////////////////////////////////
  {
    logger->beginDebugUtils("Render to depth buffer", currentFrame);
    auto renderPassInfo = render(currentFrame, renderPassDepth, frameBufferDepth);
    clearValues[0].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = clearValues.data();
    // TODO: only one depth texture?
    vkCmdBeginRenderPass(commandBuffer->getCommandBuffer()[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    // Set depth bias (aka "Polygon offset")
    // Required to avoid shadow mapping artifacts
    vkCmdSetDepthBias(commandBuffer->getCommandBuffer()[currentFrame], depthBiasConstant, 0.0f, depthBiasSlope);
    // draw scene here
    spriteManager->draw(currentFrame, SpriteRenderMode::DEPTH);
    modelManager->draw(currentFrame, ModelRenderMode::DEPTH, frameTimer);
    vkCmdEndRenderPass(commandBuffer->getCommandBuffer()[currentFrame]);

    logger->endDebugUtils(currentFrame);
  }

  /////////////////////////////////////////////////////////////////////////////////////////
  // depth to screne barrier
  /////////////////////////////////////////////////////////////////////////////////////////
  // Image memory barrier to make sure that writes are finished before sampling from the texture
  VkImageMemoryBarrier imageMemoryBarrier = {};
  imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  // We won't be changing the layout of the image
  imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  imageMemoryBarrier.image = resultDepth[currentFrame]->getImage()->getImage();
  imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
  imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
  /////////////////////////////////////////////////////////////////////////////////////////
  // render to screen
  /////////////////////////////////////////////////////////////////////////////////////////
  {
    logger->beginDebugUtils("Render to screen", currentFrame);

    auto renderPassInfo = render(imageIndex, renderPass, frameBuffer);
    clearValues[0].color = {0.25f, 0.25f, 0.25f, 1.f};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer->getCommandBuffer()[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    lightManager->draw(currentFrame);
    // draw scene here
    spriteManager->draw(currentFrame, SpriteRenderMode::FULL);
    modelManager->draw(currentFrame, ModelRenderMode::FULL, frameTimer);

    debugVisualization->draw(currentFrame);

    gui->updateBuffers(currentFrame);
    gui->drawFrame(currentFrame, commandBuffer->getCommandBuffer()[currentFrame]);
    vkCmdEndRenderPass(commandBuffer->getCommandBuffer()[currentFrame]);

    logger->endDebugUtils(currentFrame);
  }
  ///////////////////////////////////////////////////////////////////////////////////////////

  if (vkEndCommandBuffer(commandBuffer->getCommandBuffer()[currentFrame]) != VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
  }
  //
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

  result = vkQueueSubmit(queue->getGraphicQueue(), 1, &submitInfo, inFlightFences[currentFrame]->getFence());
  if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to submit draw command buffer!");
  }

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapChains[] = {swapchain->getSwapchain()};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;

  presentInfo.pImageIndices = &imageIndex;

  result = vkQueuePresentKHR(queue->getPresentQueue(), &presentInfo);

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
  int frame = 0;
  while (!glfwWindowShouldClose(window->getWindow())) {
    auto startTimeCurrent = std::chrono::high_resolution_clock::now();
    glfwPollEvents();
    drawFrame();
    frame++;
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsedCurrent = std::chrono::duration_cast<std::chrono::microseconds>(end - startTimeCurrent).count();
    frameTimer = (float)elapsedCurrent / 1000000.f;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - startTime).count();
    if (elapsed > 1000000.f) {
      fps = (float)frame * (1000000.f / elapsed);
      startTime = end;
      frame = 0;
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