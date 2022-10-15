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

constexpr int MAX_FRAMES_IN_FLIGHT = 2;
std::tuple<int, int> resolution = {800, 600};
const std::vector<Vertex> vertices = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                      {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                      {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                      {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};

const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

std::shared_ptr<Window> window;
std::shared_ptr<Instance> instance;
std::shared_ptr<Device> device;
std::shared_ptr<Swapchain> swapchain;
std::shared_ptr<RenderPass> renderPass;
std::shared_ptr<Pipeline> pipeline;
std::shared_ptr<Framebuffer> framebuffer;
std::shared_ptr<CommandPool> commandPool;
std::shared_ptr<Queue> queue;
std::shared_ptr<VertexBuffer> vertexBuffer;
std::shared_ptr<IndexBuffer> indexBuffer;
std::shared_ptr<UniformBuffer> uniformBuffer;
std::shared_ptr<DescriptorPool> descriptorPool;
std::shared_ptr<DescriptorSet> descriptorSet;
std::shared_ptr<CommandBuffer> commandBuffer;
std::array<std::shared_ptr<Semaphore>, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores, renderFinishedSemaphores;
std::array<std::shared_ptr<Fence>, MAX_FRAMES_IN_FLIGHT> inFlightFences;

uint64_t currentFrame = 0;

void initialize() {
  window = std::make_shared<Window>(resolution);
  instance = std::make_shared<Instance>("Vulkan", true, window);
  std::shared_ptr<Surface> surface = std::make_shared<Surface>(window, instance);
  device = std::make_shared<Device>(surface, instance);
  swapchain = std::make_shared<Swapchain>(window, surface, device);
  renderPass = std::make_shared<RenderPass>(swapchain, device);
  std::shared_ptr<DescriptorSetLayout> descriptorSetLayout = std::make_shared<DescriptorSetLayout>(device);
  std::shared_ptr<Shader> shader = std::make_shared<Shader>("../shaders/simple_vertex.spv",
                                                            "../shaders/simple_fragment.spv", device);
  pipeline = std::make_shared<Pipeline>(shader, descriptorSetLayout, renderPass, device);

  framebuffer = std::make_shared<Framebuffer>(resolution, swapchain, renderPass, device);

  commandPool = std::make_shared<CommandPool>(device);
  queue = std::make_shared<Queue>(device);

  vertexBuffer = std::make_shared<VertexBuffer>(vertices, commandPool, queue, device);
  indexBuffer = std::make_shared<IndexBuffer>(indices, commandPool, queue, device);
  uniformBuffer = std::make_shared<UniformBuffer>(MAX_FRAMES_IN_FLIGHT, commandPool, queue, device);

  descriptorPool = std::make_shared<DescriptorPool>(MAX_FRAMES_IN_FLIGHT, device);
  descriptorSet = std::make_shared<DescriptorSet>(MAX_FRAMES_IN_FLIGHT, uniformBuffer, descriptorSetLayout,
                                                  descriptorPool, device);
  commandBuffer = std::make_shared<CommandBuffer>(MAX_FRAMES_IN_FLIGHT, commandPool, device);
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    imageAvailableSemaphores[i] = std::make_shared<Semaphore>(device);
    renderFinishedSemaphores[i] = std::make_shared<Semaphore>(device);
    inFlightFences[i] = std::make_shared<Fence>(device);
  }
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

  UniformBufferObject ubo{};
  ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.proj = glm::perspective(glm::radians(45.0f),
                              swapchain->getSwapchainExtent().width / (float)swapchain->getSwapchainExtent().height,
                              0.1f, 10.0f);
  ubo.proj[1][1] *= -1;

  void* data;
  vkMapMemory(device->getLogicalDevice(), uniformBuffer->getBuffer()[currentFrame]->getMemory(), 0, sizeof(ubo), 0,
              &data);
  memcpy(data, &ubo, sizeof(ubo));
  vkUnmapMemory(device->getLogicalDevice(), uniformBuffer->getBuffer()[currentFrame]->getMemory());
  //

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

  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->getPipeline());

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)swapchain->getSwapchainExtent().width;
  viewport.height = (float)swapchain->getSwapchainExtent().height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = swapchain->getSwapchainExtent();
  vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  VkBuffer vertexBuffers[] = {vertexBuffer->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame], indexBuffer->getBuffer()->getData(), 0,
                       VK_INDEX_TYPE_UINT16);

  vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline->getPipelineLayout(), 0, 1, &descriptorSet->getDescriptorSets()[currentFrame], 0,
                          nullptr);

  vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

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

  currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
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