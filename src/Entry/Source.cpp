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

const std::vector<Vertex> vertices = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                      {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                      {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                      {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};

const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

struct UniformObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

std::shared_ptr<Window> window;
std::shared_ptr<Instance> instance;
std::shared_ptr<Device> device;
std::shared_ptr<Swapchain> swapchain;
std::shared_ptr<RenderPass> renderPass;
std::shared_ptr<Framebuffer> framebuffer;
std::shared_ptr<CommandPool> commandPool;
std::shared_ptr<Queue> queue;
std::shared_ptr<DescriptorPool> descriptorPool;

std::shared_ptr<CommandBuffer> commandBuffer;
std::vector<std::shared_ptr<Semaphore>> imageAvailableSemaphores, renderFinishedSemaphores;
std::vector<std::shared_ptr<Fence>> inFlightFences;

std::shared_ptr<Settings> settings;
std::shared_ptr<SpriteManager> spriteManager;
std::shared_ptr<Sprite> sprite1, sprite2;

uint64_t currentFrame = 0;

void initialize() {
  settings = std::make_shared<Settings>(std::tuple{800, 600}, 2);
  window = std::make_shared<Window>(settings->getResolution());
  instance = std::make_shared<Instance>("Vulkan", true, window);
  std::shared_ptr<Surface> surface = std::make_shared<Surface>(window, instance);
  device = std::make_shared<Device>(surface, instance);
  swapchain = std::make_shared<Swapchain>(window, surface, device);
  renderPass = std::make_shared<RenderPass>(swapchain, device);
  framebuffer = std::make_shared<Framebuffer>(settings->getResolution(), swapchain, renderPass, device);
  commandPool = std::make_shared<CommandPool>(device);
  queue = std::make_shared<Queue>(device);
  commandBuffer = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(), commandPool, device);
  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    imageAvailableSemaphores.push_back(std::make_shared<Semaphore>(device));
    renderFinishedSemaphores.push_back(std::make_shared<Semaphore>(device));
    inFlightFences.push_back(std::make_shared<Fence>(device));
  }

  std::shared_ptr<Texture> texture = std::make_shared<Texture>("../data/statue.jpg", commandPool, queue, device);
  spriteManager = std::make_shared<SpriteManager>(commandPool, commandBuffer, queue, renderPass, device, settings);
  sprite1 = spriteManager->createSprite(texture);
  spriteManager->registerSprite(sprite1);
  sprite2 = spriteManager->createSprite(texture);
  spriteManager->registerSprite(sprite2);
  auto view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  auto proj = glm::perspective(glm::radians(45.0f),
                               swapchain->getSwapchainExtent().width / (float)swapchain->getSwapchainExtent().height,
                               0.1f, 10.0f);
  proj[1][1] *= -1;
  sprite1->setView(view);
  sprite1->setProjection(proj);

  sprite2->setView(view);
  sprite2->setProjection(proj);
}

void drawFrame() {
  vkWaitForFences(device->getLogicalDevice(), 1, &inFlightFences[currentFrame]->getFence(), VK_TRUE, UINT64_MAX);

  uint32_t imageIndex;
  VkResult result = vkAcquireNextImageKHR(device->getLogicalDevice(), swapchain->getSwapchain(), UINT64_MAX,
                                          imageAvailableSemaphores[currentFrame]->getSemaphore(), VK_NULL_HANDLE,
                                          &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    // TODO: recreate swapchain
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  // Update uniform
  static auto startTime = std::chrono::high_resolution_clock::now();

  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

  auto model1 = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  auto model2 = glm::translate(glm::mat4(1.f), glm::vec3(-1.f, 1.f, 0.f));

  sprite1->setModel(model1);
  sprite2->setModel(model2);

  vkResetFences(device->getLogicalDevice(), 1, &inFlightFences[currentFrame]->getFence());
  vkResetCommandBuffer(commandBuffer->getCommandBuffer()[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);

  // record command buffer
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if (vkBeginCommandBuffer(commandBuffer->getCommandBuffer()[currentFrame], &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }

  VkRenderPassBeginInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = renderPass->getRenderPass();
  renderPassInfo.framebuffer = framebuffer->getBuffer()[imageIndex];
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = swapchain->getSwapchainExtent();

  VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearColor;

  vkCmdBeginRenderPass(commandBuffer->getCommandBuffer()[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  spriteManager->draw(currentFrame);
  vkCmdEndRenderPass(commandBuffer->getCommandBuffer()[currentFrame]);

  if (vkEndCommandBuffer(commandBuffer->getCommandBuffer()[currentFrame]) != VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
  }
  //
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]->getSemaphore()};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer->getCommandBuffer()[currentFrame];

  VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]->getSemaphore()};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  if (vkQueueSubmit(queue->getGraphicQueue(), 1, &submitInfo, inFlightFences[currentFrame]->getFence()) != VK_SUCCESS) {
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
  while (!glfwWindowShouldClose(window->getWindow())) {
    glfwPollEvents();
    drawFrame();
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