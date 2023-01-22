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

#include "OffscreenPart.h"
#include "ComputePart.h"
#include "ScreenPart.h"

float fps = 0;
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
std::shared_ptr<Sprite> sprite1, sprite2;
std::shared_ptr<Model3DManager> modelManager;
std::shared_ptr<Model3D> model3D;

std::shared_ptr<GUI> gui;
std::shared_ptr<ComputePart> computePart;
std::shared_ptr<ScreenPart> screenPart;

void initializeCompute() {
  computePart = std::make_shared<ComputePart>(device, queue, commandBuffer, commandPool, settings);
}

void initializeScreen() {
  screenPart = std::make_shared<ScreenPart>(computePart->getResultTextures(), window, surface, device, queue,
                                            commandPool, commandBuffer, settings);
}

PFN_vkCmdBeginDebugUtilsLabelEXT CmdBeginDebugUtilsLabelEXT;
PFN_vkCmdEndDebugUtilsLabelEXT CmdEndDebugUtilsLabelEXT;
PFN_vkSetDebugUtilsObjectNameEXT SetDebugUtilsObjectNameEXT;

void initialize() {
  clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  clearValues[1].depthStencil = {1.0f, 0};

  settings = std::make_shared<Settings>(std::tuple{800, 592}, 2);
  window = std::make_shared<Window>(settings->getResolution());
  Input::initialize(window);
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

  initializeCompute();
  initializeScreen();

  gui = std::make_shared<GUI>(settings->getResolution(), window, device);
  gui->initialize(screenPart->getRenderPass(), queue, commandPool);
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
  result = vkAcquireNextImageKHR(device->getLogicalDevice(), screenPart->getSwapchain()->getSwapchain(), UINT64_MAX,
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
  gui->addCheckbox("Compute", {20, 80}, {100, 60}, computePart->getCheckboxes());
  gui->updateBuffers(currentFrame);

  // record command buffer
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if (vkBeginCommandBuffer(commandBuffer->getCommandBuffer()[currentFrame], &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }

  // Set a name for the command buffer
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
  markerInfo.pLabelName = "Raytraycing compute";
  CmdBeginDebugUtilsLabelEXT(commandBuffer->getCommandBuffer()[currentFrame], &markerInfo);

  /////////////////////////////////////////////////////////////////////////////////////////
  // compute
  /////////////////////////////////////////////////////////////////////////////////////////
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
                    computePart->getPipeline()->getPipeline());
  computePart->draw(currentFrame);

  CmdEndDebugUtilsLabelEXT(commandBuffer->getCommandBuffer()[currentFrame]);

  markerInfo = {};
  markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  markerInfo.pLabelName = "Compute-Render sync";
  CmdBeginDebugUtilsLabelEXT(commandBuffer->getCommandBuffer()[currentFrame], &markerInfo);

  /////////////////////////////////////////////////////////////////////////////////////////
  // compute to graphic barrier
  /////////////////////////////////////////////////////////////////////////////////////////
  // Image memory barrier to make sure that compute shader writes are finished before sampling from the texture
  VkImageMemoryBarrier imageMemoryBarrier = {};
  imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  // We won't be changing the layout of the image
  imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  imageMemoryBarrier.image = computePart->getResultTextures()[currentFrame]->getImageView()->getImage()->getImage();
  imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

  CmdEndDebugUtilsLabelEXT(commandBuffer->getCommandBuffer()[currentFrame]);

  markerInfo = {};
  markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  markerInfo.pLabelName = "Render to screen";
  CmdBeginDebugUtilsLabelEXT(commandBuffer->getCommandBuffer()[currentFrame], &markerInfo);
  /////////////////////////////////////////////////////////////////////////////////////////
  // render to screen
  /////////////////////////////////////////////////////////////////////////////////////////
  {
    auto renderPassInfo = render(imageIndex, screenPart->getRenderPass(), screenPart->getFramebuffer(),
                                 screenPart->getSwapchain());

    vkCmdBeginRenderPass(commandBuffer->getCommandBuffer()[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    for (auto sprite : screenPart->getSprites()) {
      screenPart->getSpriteManager()->unregisterSprite(sprite);
    }
    screenPart->getSpriteManager()->registerSprite(screenPart->getSprites()[currentFrame]);
    screenPart->getSpriteManager()->draw(currentFrame);
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

  VkSwapchainKHR swapChains[] = {screenPart->getSwapchain()->getSwapchain()};
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
    glfwPollEvents();
    drawFrame();
    frame++;
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - startTime).count();
    if (elapsed > 1000.f) {
      fps = (float)frame * (1000.f / elapsed);
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