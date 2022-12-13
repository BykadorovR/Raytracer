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

#include "OffscreenPart.h"

float fps = 0;

std::shared_ptr<Window> window;
std::shared_ptr<Instance> instance;
std::shared_ptr<Device> device;
std::shared_ptr<Swapchain> swapchain;
std::shared_ptr<RenderPass> renderPass;
std::shared_ptr<RenderPass> renderPassGUI;
std::shared_ptr<Framebuffer> framebuffer;
std::shared_ptr<CommandPool> commandPool;
std::shared_ptr<Queue> queue;

std::shared_ptr<CommandBuffer> commandBuffer;
std::vector<std::shared_ptr<Semaphore>> imageAvailableSemaphores, renderFinishedSemaphores;
std::vector<std::shared_ptr<Fence>> inFlightFences;

std::shared_ptr<Settings> settings;
std::shared_ptr<SpriteManager> spriteManager, spriteManagerScreen;
std::shared_ptr<Sprite> sprite1, sprite2;
std::vector<std::shared_ptr<Sprite>> spriteScreen;
std::shared_ptr<Model3DManager> modelManager;
std::shared_ptr<Model3D> model3D;
std::shared_ptr<Surface> surface;
std::shared_ptr<GUI> gui;

uint64_t currentFrame = 0;
std::vector<std::shared_ptr<Texture>> computeTextures;
std::shared_ptr<Pipeline> computePipeline;
std::shared_ptr<ImageView> depthView;
std::shared_ptr<DescriptorSet> computeDescriptorSet;
std::shared_ptr<DescriptorPool> descriptorPool;

std::shared_ptr<OffscreenPart> offscreenPart;

void initializeScene() {
  auto shader = std::make_shared<Shader>(device);
  // Initialize scene
  std::shared_ptr<Texture> texture = std::make_shared<Texture>("../data/statue.jpg", commandPool, queue, device);
  shader->add("../shaders/simple_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader->add("../shaders/simple_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  spriteManager = std::make_shared<SpriteManager>(shader, commandPool, commandBuffer, queue,
                                                  offscreenPart->getRenderPass(), device, settings);

  sprite1 = spriteManager->createSprite(texture);
  spriteManager->registerSprite(sprite1);
  sprite2 = spriteManager->createSprite(texture);
  spriteManager->registerSprite(sprite2);

  auto view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  auto proj = glm::perspective(
      glm::radians(45.0f),
      (float)std::get<0>(settings->getResolution()) / (float)std::get<1>(settings->getResolution()), 0.1f, 10.0f);
  proj[1][1] *= -1;
  sprite1->setView(view);
  sprite1->setProjection(proj);

  sprite2->setView(view);
  sprite2->setProjection(proj);

  std::shared_ptr<Texture> textureModel = std::make_shared<Texture>("../data/viking_room.png", commandPool, queue,
                                                                    device);
  modelManager = std::make_shared<Model3DManager>(commandPool, commandBuffer, queue, offscreenPart->getRenderPass(),
                                                  device, settings);
  model3D = modelManager->createModel("../data/viking_room.obj", textureModel);
  modelManager->registerModel(model3D);

  model3D->setView(view);
  model3D->setProjection(proj);
}

void initializeOffscreen() {
  offscreenPart = std::make_shared<OffscreenPart>(device, queue, commandPool, commandBuffer, settings);
}

void initializeCompute() {
  // allocate textures where to store compute shader result
  computeTextures.resize(settings->getMaxFramesInFlight());
  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    // Image will be sampled in the fragment shader and used as storage target in the compute shader
    std::shared_ptr<Image> image = std::make_shared<Image>(
        settings->getResolution(), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, device);
    image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, commandPool, queue);
    std::shared_ptr<ImageView> imageView = std::make_shared<ImageView>(image, VK_IMAGE_ASPECT_COLOR_BIT, device);
    std::shared_ptr<Texture> texture = std::make_shared<Texture>(imageView, device);
    computeTextures[i] = texture;
  }

  std::shared_ptr<DescriptorSetLayout> computeDescriptorSetLayout = std::make_shared<DescriptorSetLayout>(device);
  computeDescriptorSetLayout->createCompute();
  auto shader = std::make_shared<Shader>(device);
  shader->add("../shaders/grayscale_compute.spv", VK_SHADER_STAGE_COMPUTE_BIT);
  computePipeline = std::make_shared<Pipeline>(shader, computeDescriptorSetLayout, device);
  computePipeline->createCompute();

  descriptorPool = std::make_shared<DescriptorPool>(100, device);
  computeDescriptorSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), computeDescriptorSetLayout,
                                                         descriptorPool, device);
  computeDescriptorSet->createCompute(offscreenPart->getResultTextures(), computeTextures);
}

void initializeScreen() {
  swapchain = std::make_shared<Swapchain>(window, surface, device);
  renderPass = std::make_shared<RenderPass>(swapchain->getImageFormat(), device);
  renderPass->initialize();

  framebuffer = std::make_shared<Framebuffer>(settings->getResolution(), swapchain->getImageViews(),
                                              swapchain->getDepthImageView(), renderPass, device);

  auto shaderGray = std::make_shared<Shader>(device);
  shaderGray->add("../shaders/final_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shaderGray->add("../shaders/final_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  spriteManagerScreen = std::make_shared<SpriteManager>(shaderGray, commandPool, commandBuffer, queue, renderPass,
                                                        device, settings);
  spriteScreen.resize(settings->getMaxFramesInFlight());
  for (int i = 0; i < spriteScreen.size(); i++) {
    spriteScreen[i] = spriteManagerScreen->createSprite(computeTextures[i]);
  }
}

void initialize() {
  settings = std::make_shared<Settings>(std::tuple{800, 592}, 2);
  window = std::make_shared<Window>(settings->getResolution());
  instance = std::make_shared<Instance>("Vulkan", true, window);
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

  initializeOffscreen();
  initializeScene();
  initializeCompute();
  initializeScreen();

  gui = std::make_shared<GUI>(settings->getResolution(), window, device);
  gui->initialize(renderPass, queue, commandPool);
}

void drawFrame() {
  vkWaitForFences(device->getLogicalDevice(), 1, &inFlightFences[currentFrame]->getFence(), VK_TRUE, UINT64_MAX);

  uint32_t imageIndex;
  // RETURNS ONLY INDEX, NOT IMAGE
  VkResult result = vkAcquireNextImageKHR(device->getLogicalDevice(), swapchain->getSwapchain(), UINT64_MAX,
                                          imageAvailableSemaphores[currentFrame]->getSemaphore(), VK_NULL_HANDLE,
                                          &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    // TODO: recreate swapchain
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  vkResetFences(device->getLogicalDevice(), 1, &inFlightFences[currentFrame]->getFence());
  vkResetCommandBuffer(commandBuffer->getCommandBuffer()[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);

  gui->addText("FPS", {20, 20}, {100, 60}, {std::to_string(fps)});
  gui->updateBuffers(currentFrame);

  // record command buffer
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  if (vkBeginCommandBuffer(commandBuffer->getCommandBuffer()[currentFrame], &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }
  ////////////////////////////////////////////////////////////////////////
  // render scene
  ////////////////////////////////////////////////////////////////////////
  {
    // Update uniform
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    auto model1 = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    auto model2 = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -0.5f));
    auto model3 = glm::translate(glm::mat4(1.f), glm::vec3(1.f, -1.f, 0.f));
    model3 = glm::rotate(model3, time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    sprite1->setModel(model1);
    sprite2->setModel(model2);
    model3D->setModel(model3);
    // render
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = offscreenPart->getRenderPass()->getRenderPass();
    renderPassInfo.framebuffer = offscreenPart->getFramebuffer()->getBuffer()[currentFrame];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchain->getSwapchainExtent();

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer->getCommandBuffer()[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    spriteManager->draw(currentFrame);
    modelManager->draw(currentFrame);
    vkCmdEndRenderPass(commandBuffer->getCommandBuffer()[currentFrame]);
  }

  /////////////////////////////////////////////////////////////////////////////////////////
  // graphic to compute barrier
  /////////////////////////////////////////////////////////////////////////////////////////
  {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = offscreenPart->getResultTextures()[currentFrame]->getImageView()->getImage()->getImage();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[currentFrame], sourceStage, destinationStage, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);
  }

  /////////////////////////////////////////////////////////////////////////////////////////
  // compute
  /////////////////////////////////////////////////////////////////////////////////////////
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
                    computePipeline->getPipeline());
  vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
                          computePipeline->getPipelineLayout(), 0, 1,
                          &computeDescriptorSet->getDescriptorSets()[currentFrame], 0, 0);

  vkCmdDispatch(commandBuffer->getCommandBuffer()[currentFrame], std::get<0>(settings->getResolution()) / 16,
                std::get<1>(settings->getResolution()) / 16, 1);

  /////////////////////////////////////////////////////////////////////////////////////////
  // compute to graphic barrier
  /////////////////////////////////////////////////////////////////////////////////////////
  // Image memory barrier to make sure that compute shader writes are finished before sampling from the texture
  VkImageMemoryBarrier imageMemoryBarrier = {};
  imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  // We won't be changing the layout of the image
  imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  imageMemoryBarrier.image = computeTextures[currentFrame]->getImageView()->getImage()->getImage();
  imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
  /////////////////////////////////////////////////////////////////////////////////////////
  // render to screen
  /////////////////////////////////////////////////////////////////////////////////////////
  {
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass->getRenderPass();
    renderPassInfo.framebuffer = framebuffer->getBuffer()[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchain->getSwapchainExtent();

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer->getCommandBuffer()[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    for (auto sprite : spriteScreen) {
      spriteManagerScreen->unregisterSprite(sprite);
    }
    spriteManagerScreen->registerSprite(spriteScreen[currentFrame]);
    spriteManagerScreen->draw(currentFrame);
    gui->drawFrame(currentFrame, commandBuffer->getCommandBuffer()[currentFrame]);

    vkCmdEndRenderPass(commandBuffer->getCommandBuffer()[currentFrame]);
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