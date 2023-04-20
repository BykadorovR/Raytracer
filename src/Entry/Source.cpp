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

float fps = 0;
float frameTimer = 0.f;
uint64_t currentFrame = 0;

std::shared_ptr<Window> window;
std::shared_ptr<Instance> instance;
std::shared_ptr<Device> device;
std::shared_ptr<CommandBuffer> commandBuffer;
std::shared_ptr<CommandPool> commandPool;
std::shared_ptr<Queue> queue;
std::shared_ptr<Surface> surface;
std::shared_ptr<Settings> settings;
std::array<VkClearValue, 2> clearValues{};

std::vector<std::shared_ptr<Semaphore>> imageAvailableSemaphores, renderFinishedSemaphores;
std::vector<std::shared_ptr<Fence>> inFlightFences;

std::shared_ptr<SpriteManager> spriteManager;
std::shared_ptr<Model3DManager> modelManager;
std::shared_ptr<ModelOBJ> model3D;
std::shared_ptr<ModelGLTF> modelGLTF;
std::shared_ptr<Input> input;
std::shared_ptr<GUI> gui;
std::shared_ptr<Swapchain> swapchain;
std::shared_ptr<RenderPass> renderPass;
std::shared_ptr<Framebuffer> frameBuffer;
std::shared_ptr<CameraFly> camera;
std::shared_ptr<LightManager> lightManager;
std::shared_ptr<PhongLight> phongLightHorizontal, phongLightVertical;
std::shared_ptr<DebugVisualization> debugVisualization;

PFN_vkCmdBeginDebugUtilsLabelEXT CmdBeginDebugUtilsLabelEXT;
PFN_vkCmdEndDebugUtilsLabelEXT CmdEndDebugUtilsLabelEXT;
PFN_vkSetDebugUtilsObjectNameEXT SetDebugUtilsObjectNameEXT;

void initialize() {
  clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  clearValues[1].depthStencil = {1.0f, 0};

  settings = std::make_shared<Settings>(std::tuple{800, 592}, 2);
  window = std::make_shared<Window>(settings->getResolution());
  input = std::make_shared<Input>(window);

  instance = std::make_shared<Instance>("Vulkan", true, window);
  CmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance->getInstance(),
                                                                                       "vkCmdBeginDebugUtilsLabelEXT");
  CmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance->getInstance(),
                                                                                   "vkCmdEndDebugUtilsLabelEXT");
  SetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance->getInstance(),
                                                                                       "vkSetDebugUtilsObjectNameEXT");

  surface = std::make_shared<Surface>(window, instance);
  device = std::make_shared<Device>(surface, instance);
  commandPool = std::make_shared<CommandPool>(device);
  queue = std::make_shared<Queue>(device);
  commandBuffer = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(), commandPool, device);
  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    imageAvailableSemaphores.push_back(std::make_shared<Semaphore>(device));
    renderFinishedSemaphores.push_back(std::make_shared<Semaphore>(device));
    inFlightFences.push_back(std::make_shared<Fence>(device));
  }

  swapchain = std::make_shared<Swapchain>(window, surface, device);
  renderPass = std::make_shared<RenderPass>(swapchain->getImageFormat(), device);
  renderPass->initialize();
  frameBuffer = std::make_shared<Framebuffer>(settings->getResolution(), swapchain->getImageViews(),
                                              swapchain->getDepthImageView(), renderPass, device);
  auto shader3D = std::make_shared<Shader>(device);
  shader3D->add("../shaders/simple3D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader3D->add("../shaders/simple3D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  auto shader2D = std::make_shared<Shader>(device);
  shader2D->add("../shaders/simple2D_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader2D->add("../shaders/simple2D_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

  gui = std::make_shared<GUI>(settings->getResolution(), window, device);
  gui->initialize(renderPass, queue, commandPool);

  auto texture = std::make_shared<Texture>("../data/statue.jpg", commandPool, queue, device);
  auto normalMap = std::make_shared<Texture>("../data/brickwall_normal.jpg", commandPool, queue, device);
  camera = std::make_shared<CameraFly>(settings);
  input->subscribe(std::dynamic_pointer_cast<InputSubscriber>(camera));
  input->subscribe(std::dynamic_pointer_cast<InputSubscriber>(gui));
  lightManager = std::make_shared<LightManager>(settings, device);
  phongLightHorizontal = lightManager->createPhongLight(glm::vec3(0.f, 0.f, 3.f), glm::vec3(1.f, 1.f, 1.f), 0.1f, 1.f);
  phongLightVertical = lightManager->createPhongLight(glm::vec3(0.f, 0.f, 3.f), glm::vec3(1.f, 1.f, 1.f), 0.1f, 1.f);

  spriteManager = std::make_shared<SpriteManager>(shader2D, lightManager, commandPool, commandBuffer, queue, renderPass,
                                                  device, settings);
  modelManager = std::make_shared<Model3DManager>(commandPool, commandBuffer, queue, renderPass, device, settings);
  debugVisualization = std::make_shared<DebugVisualization>(modelManager, camera, gui);
  debugVisualization->setLights(lightManager);
  input->subscribe(std::dynamic_pointer_cast<InputSubscriber>(debugVisualization));

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

  // model3D = modelManager->createModel("../data/viking_room.obj");
  // modelGLTF = modelManager->createModelGLTF("../data/Avocado/Avocado.gltf");
  // modelGLTF = modelManager->createModelGLTF("../data/CesiumMan/CesiumMan.gltf");
  // modelGLTF = modelManager->createModelGLTF("../data/BrainStem/BrainStem.gltf");
  // modelGLTF = modelManager->createModelGLTF("../data/SimpleSkin/SimpleSkin.gltf");
  // modelGLTF = modelManager->createModelGLTF("../data/Sponza/Sponza.gltf");
  // modelGLTF = modelManager->createModelGLTF("../data/DamagedHelmet/DamagedHelmet.gltf");
  //{
  //  glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(-2.f, -1.f, 0.f));
  //  // model = glm::rotate(model, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
  //  model3D->setModel(model);
  //}
  //{
  //  glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(2.f, -1.f, 0.f));
  //  // model = glm::scale(model, glm::vec3(15.f, 15.f, 15.f));
  //  modelGLTF->setModel(model);
  //}
  spriteManager->registerSprite(sprite);
  spriteManager->registerSprite(sprite2);
  spriteManager->registerSprite(sprite3);
  spriteManager->registerSprite(sprite4);
  spriteManager->registerSprite(sprite5);
  spriteManager->registerSprite(sprite6);
  /*modelManager->registerModel(model3D);
  modelManager->registerModelGLTF(modelGLTF);*/
}

VkRenderPassBeginInfo render(int index,
                             std::shared_ptr<RenderPass> renderPass,
                             std::shared_ptr<Framebuffer> framebuffer,
                             std::shared_ptr<Swapchain> swapchain) {
  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = renderPass->getRenderPass();
  renderPassInfo.framebuffer = framebuffer->getBuffer()[index];
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = swapchain->getSwapchainExtent();

  renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

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

  gui->addText("FPS", {20, 20}, {100, 60}, {std::to_string(fps)});
  // record command buffer
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if (vkBeginCommandBuffer(commandBuffer->getCommandBuffer()[currentFrame], &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }

  VkDebugUtilsObjectNameInfoEXT cmdBufInfo = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .pNext = NULL,
      .objectType = VK_OBJECT_TYPE_COMMAND_BUFFER,
      .objectHandle = (uint64_t)commandBuffer->getCommandBuffer()[currentFrame],
      .pObjectName = "Raytracing command buffer",
  };
  SetDebugUtilsObjectNameEXT(device->getLogicalDevice(), &cmdBufInfo);

  VkDebugUtilsLabelEXT markerInfo = {};
  markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  markerInfo.pLabelName = "Render to screen";
  CmdBeginDebugUtilsLabelEXT(commandBuffer->getCommandBuffer()[currentFrame], &markerInfo);
  /////////////////////////////////////////////////////////////////////////////////////////
  // render to screen
  /////////////////////////////////////////////////////////////////////////////////////////
  {
    auto renderPassInfo = render(imageIndex, renderPass, frameBuffer, swapchain);

    vkCmdBeginRenderPass(commandBuffer->getCommandBuffer()[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    static float angleHorizontal = 90.f;
    glm::vec3 lightPositionHorizontal = glm::vec3(3.f * cos(glm::radians(angleHorizontal)), 0.f,
                                                  3.f * sin(glm::radians(angleHorizontal)));
    static float angleVertical = 0.f;
    glm::vec3 lightPositionVertical = glm::vec3(0.f, 3.f * sin(glm::radians(angleVertical)),
                                                3.f * cos(glm::radians(angleVertical)));

    phongLightHorizontal->setPosition(lightPositionHorizontal);
    phongLightVertical->setPosition(lightPositionVertical);
    angleVertical += 0.01f;
    angleHorizontal += 0.01f;

    lightManager->draw(currentFrame);
    // draw scene here
    spriteManager->setCamera(camera);
    spriteManager->draw(currentFrame);

    /*model3D->setProjection(camera->getProjection());
    model3D->setView(camera->getView());
    modelGLTF->setProjection(camera->getProjection());
    modelGLTF->setView(camera->getView());*/
    modelManager->draw(frameTimer, currentFrame);
    modelManager->drawGLTF(frameTimer, currentFrame);

    debugVisualization->draw();

    gui->updateBuffers(currentFrame);
    gui->drawFrame(currentFrame, commandBuffer->getCommandBuffer()[currentFrame]);
    vkCmdEndRenderPass(commandBuffer->getCommandBuffer()[currentFrame]);
  }
  ///////////////////////////////////////////////////////////////////////////////////////////

  CmdEndDebugUtilsLabelEXT(commandBuffer->getCommandBuffer()[currentFrame]);

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