#include <iostream>
#include <ostream>
#include <fstream>
#include <chrono>
#include <future>
#include <windows.h>
#undef near
#undef far

#include "Window.h"
#include "Instance.h"
#include "Surface.h"
#include "Device.h"
#include "Swapchain.h"
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
#include "Line.h"
#include "Camera.h"
#include "LightManager.h"
#include "Terrain.h"
#include "DebugVisualization.h"
#include "State.h"
#include "Logger.h"
#include "BS_thread_pool.hpp"
#include "ParticleSystem.h"
#include "Postprocessing.h"

#undef near
#undef far

float fps = 0;
float frameTimer = 0.f;
uint64_t currentFrame = 0;
uint64_t globalFrame = 0;
// Depth bias (and slope) are used to avoid shadowing artifacts
// Constant depth bias factor (always applied)
float depthBiasConstant = 1.25f;
// Slope depth bias factor, applied depending on polygon's slope
float depthBiasSlope = 1.75f;

std::shared_ptr<Window> window;
std::shared_ptr<Instance> instance;
std::shared_ptr<Device> device;
std::shared_ptr<CommandBuffer> commandBuffer, commandBufferTransfer, commandBufferParticleSystem,
    commandBufferPostprocessing, commandBufferGUI;
std::shared_ptr<CommandPool> commandPool, commandPoolTransfer, commandPoolParticleSystem, commandPoolPostprocessing,
    commandPoolGUI;
std::shared_ptr<DescriptorPool> descriptorPool;
std::shared_ptr<Surface> surface;
std::shared_ptr<Settings> settings;
std::shared_ptr<Terrain> terrain;
std::shared_ptr<State> state;

std::vector<std::shared_ptr<Semaphore>> imageAvailableSemaphores, renderFinishedSemaphores, particleSystemSemaphore,
    semaphorePostprocessing, semaphoreGUI;
std::vector<std::shared_ptr<Fence>> inFlightFences, particleSystemFences;
std::vector<std::vector<std::shared_ptr<Fence>>> shadowFences;

std::shared_ptr<SpriteManager> spriteManager;
std::shared_ptr<Model3DManager> modelManager;
std::shared_ptr<ModelGLTF> modelGLTF;
std::shared_ptr<ParticleSystem> particleSystem;
std::shared_ptr<Postprocessing> postprocessing;
std::shared_ptr<Input> input;
std::shared_ptr<GUI> gui;
std::shared_ptr<Swapchain> swapchain;
std::shared_ptr<CameraFly> camera;
std::shared_ptr<LightManager> lightManager;
std::shared_ptr<PointLight> pointLightHorizontal, pointLightVertical, pointLightHorizontal2, pointLightVertical2;
std::shared_ptr<DirectionalLight> directionalLight, directionalLight2;
std::shared_ptr<DebugVisualization> debugVisualization;
std::shared_ptr<LoggerGPU> loggerGPU, loggerCompute;
std::shared_ptr<LoggerCPU> loggerCPU;

std::future<void> updateJoints;

std::vector<std::shared_ptr<Texture>> graphicTexture;

std::shared_ptr<BS::thread_pool> pool, pool2;
std::vector<std::shared_ptr<CommandPool>> commandPoolDirectional;
std::vector<std::vector<std::shared_ptr<CommandPool>>> commandPoolPoint;

std::vector<std::shared_ptr<CommandBuffer>> commandBufferDirectional;
std::vector<std::vector<std::shared_ptr<CommandBuffer>>> commandBufferPoint;

std::vector<std::shared_ptr<LoggerGPU>> loggerGPUDirectional;
std::vector<std::vector<std::shared_ptr<LoggerGPU>>> loggerGPUPoint;
bool terrainNormals = false;
bool terrainWireframe = false;
bool terrainPatch = false;
bool showLoD = false;
bool shouldWork = true;
std::map<int, bool> layoutChanged;
std::vector<std::shared_ptr<Semaphore>> shadowSemaphore, particleSemaphore;
std::array<std::atomic<int>, 2> shadowIndex;

void directionalLightCalculator(int index) {
  std::vector<VkFence> waitShadowFence = {shadowFences[index][currentFrame]->getFence()};

  auto result = vkWaitForFences(device->getLogicalDevice(), waitShadowFence.size(), waitShadowFence.data(), VK_TRUE,
                                UINT64_MAX);
  result = vkResetFences(device->getLogicalDevice(), waitShadowFence.size(), waitShadowFence.data());
  if (result != VK_SUCCESS) throw std::runtime_error("Can't reset fence");

  auto commandBuffer = commandBufferDirectional[index];
  auto loggerGPU = loggerGPUDirectional[index];

  // record command buffer
  commandBuffer->beginCommands(currentFrame);
  loggerGPU->setCommandBufferName("Directional command buffer", currentFrame, commandBuffer);

  loggerGPU->begin("Directional to depth buffer " + std::to_string(globalFrame), currentFrame);
  // change layout to write one
  VkImageMemoryBarrier imageMemoryBarrier{};
  imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  // We won't be changing the layout of the image
  imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  imageMemoryBarrier.image = lightManager->getDirectionalLights()[index]
                                 ->getDepthTexture()[currentFrame]
                                 ->getImageView()
                                 ->getImage()
                                 ->getImage();
  imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
  imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[currentFrame],
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &imageMemoryBarrier);
  //
  auto directionalLights = lightManager->getDirectionalLights();
  VkClearValue clearDepth;
  clearDepth.depthStencil = {1.0f, 0};
  const VkRenderingAttachmentInfo depthAttachmentInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = directionalLights[index]->getDepthTexture()[currentFrame]->getImageView()->getImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearDepth,
  };

  auto [width, height] =
      directionalLights[index]->getDepthTexture()[currentFrame]->getImageView()->getImage()->getResolution();
  VkRect2D renderArea{};
  renderArea.extent.width = width;
  renderArea.extent.height = height;
  const VkRenderingInfoKHR renderInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
      .renderArea = renderArea,
      .layerCount = 1,
      .pDepthAttachment = &depthAttachmentInfo,
  };

  // TODO: only one depth texture?
  vkCmdBeginRendering(commandBuffer->getCommandBuffer()[currentFrame], &renderInfo);
  // Set depth bias (aka "Polygon offset")
  // Required to avoid shadow mapping artifacts
  vkCmdSetDepthBias(commandBuffer->getCommandBuffer()[currentFrame], depthBiasConstant, 0.0f, depthBiasSlope);

  // draw scene here
  loggerGPU->begin("Sprites to directional depth buffer " + std::to_string(globalFrame), currentFrame);
  spriteManager->drawShadow(currentFrame, commandBuffer, LightType::DIRECTIONAL, index);
  loggerGPU->end(currentFrame);
  loggerGPU->begin("Models to directional depth buffer " + std::to_string(globalFrame), currentFrame);
  modelManager->drawShadow(currentFrame, commandBuffer, LightType::DIRECTIONAL, index);
  loggerGPU->end(currentFrame);
  loggerGPU->begin("Terrain to directional depth buffer " + std::to_string(globalFrame), currentFrame);
  terrain->drawShadow(currentFrame, commandBuffer, LightType::DIRECTIONAL, index);
  loggerGPU->end(currentFrame);

  vkCmdEndRendering(commandBuffer->getCommandBuffer()[currentFrame]);
  loggerGPU->end(currentFrame);

  {
    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // We won't be changing the layout of the image
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    imageMemoryBarrier.image = lightManager->getDirectionalLights()[index]
                                   ->getDepthTexture()[currentFrame]
                                   ->getImageView()
                                   ->getImage()
                                   ->getImage();
    imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[currentFrame],
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
  }

  uint64_t signalValue = ++shadowIndex[currentFrame];
  if (signalValue == lightManager->getDirectionalLights().size() + lightManager->getPointLights().size() * 6) {
    VkTimelineSemaphoreSubmitInfo timelineInfo{};
    timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timelineInfo.pNext = NULL;
    timelineInfo.signalSemaphoreValueCount = 1;
    timelineInfo.pSignalSemaphoreValues = &signalValue;

    VkSubmitInfo submitInfoShadow{};
    submitInfoShadow.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfoShadow.pNext = &timelineInfo;
    VkSemaphore signalShadowSemaphores[] = {shadowSemaphore[currentFrame]->getSemaphore()};
    submitInfoShadow.signalSemaphoreCount = 1;
    submitInfoShadow.pSignalSemaphores = signalShadowSemaphores;
    submitInfoShadow.commandBufferCount = 1;
    submitInfoShadow.pCommandBuffers = &commandBuffer->getCommandBuffer()[currentFrame];
    // record command buffer
    commandBuffer->endCommands(currentFrame, 1, submitInfoShadow, shadowFences[index][currentFrame]);
  } else {
    commandBuffer->endCommands(currentFrame, 1, false, shadowFences[index][currentFrame]);
  }
}

void pointLightCalculator(int index, int face) {
  std::vector<VkFence> waitShadowFence = {
      shadowFences[lightManager->getDirectionalLights().size() + index * 6 + face][currentFrame]->getFence()};

  auto result = vkWaitForFences(device->getLogicalDevice(), waitShadowFence.size(), waitShadowFence.data(), VK_TRUE,
                                UINT64_MAX);
  result = vkResetFences(device->getLogicalDevice(), waitShadowFence.size(), waitShadowFence.data());
  if (result != VK_SUCCESS) throw std::runtime_error("Can't reset fence");

  auto commandBuffer = commandBufferPoint[index][face];
  auto loggerGPU = loggerGPUPoint[index][face];
  // record command buffer
  loggerGPU->setCommandBufferName("Point " + std::to_string(index) + "x" + std::to_string(face) + " command buffer",
                                  currentFrame, commandBuffer);
  commandBuffer->beginCommands(currentFrame);
  loggerGPU->begin("Point to depth buffer " + std::to_string(globalFrame), currentFrame);
  // cubemap is the only image, rest is image views, so we need to perform change only once
  if (face == 0) {
    // change layout to write one
    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // We won't be changing the layout of the image
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    imageMemoryBarrier.image = lightManager->getPointLights()[index]
                                   ->getDepthCubemap()[currentFrame]
                                   ->getTexture()
                                   ->getImageView()
                                   ->getImage()
                                   ->getImage();
    imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[currentFrame],
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &imageMemoryBarrier);
  }
  auto pointLights = lightManager->getPointLights();
  VkClearValue clearDepth;
  clearDepth.depthStencil = {1.0f, 0};
  const VkRenderingAttachmentInfo depthAttachmentInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = pointLights[index]
                       ->getDepthCubemap()[currentFrame]
                       ->getTextureSeparate()[face]
                       ->getImageView()
                       ->getImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearDepth,
  };

  auto [width, height] = pointLights[index]
                             ->getDepthCubemap()[currentFrame]
                             ->getTextureSeparate()[face]
                             ->getImageView()
                             ->getImage()
                             ->getResolution();
  VkRect2D renderArea{};
  renderArea.extent.width = width;
  renderArea.extent.height = height;
  const VkRenderingInfo renderInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = renderArea,
      .layerCount = 1,
      .pDepthAttachment = &depthAttachmentInfo,
  };

  // TODO: only one depth texture?
  vkCmdBeginRendering(commandBuffer->getCommandBuffer()[currentFrame], &renderInfo);
  // Set depth bias (aka "Polygon offset")
  // Required to avoid shadow mapping artifacts
  vkCmdSetDepthBias(commandBuffer->getCommandBuffer()[currentFrame], depthBiasConstant, 0.0f, depthBiasSlope);

  // draw scene here
  float aspect = std::get<0>(settings->getResolution()) / std::get<1>(settings->getResolution());
  loggerGPU->begin("Sprites to point depth buffer " + std::to_string(globalFrame), currentFrame);
  spriteManager->drawShadow(currentFrame, commandBuffer, LightType::POINT, index, face);
  loggerGPU->end(currentFrame);
  loggerGPU->begin("Models to point depth buffer " + std::to_string(globalFrame), currentFrame);
  modelManager->drawShadow(currentFrame, commandBuffer, LightType::POINT, index, face);
  loggerGPU->end(currentFrame);
  loggerGPU->begin("Terrain to point depth buffer " + std::to_string(globalFrame), currentFrame);
  terrain->drawShadow(currentFrame, commandBuffer, LightType::POINT, index, face);
  loggerGPU->end(currentFrame);
  vkCmdEndRendering(commandBuffer->getCommandBuffer()[currentFrame]);
  loggerGPU->end(currentFrame);

  if (face == 0) {
    VkImageMemoryBarrier imageMemoryBarrier{};

    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // We won't be changing the layout of the image
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    imageMemoryBarrier.image = lightManager->getPointLights()[index]
                                   ->getDepthCubemap()[currentFrame]
                                   ->getTexture()
                                   ->getImageView()
                                   ->getImage()
                                   ->getImage();
    imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[currentFrame],
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
  }

  uint64_t signalValue = ++shadowIndex[currentFrame];
  if (signalValue == lightManager->getDirectionalLights().size() + lightManager->getPointLights().size() * 6) {
    VkTimelineSemaphoreSubmitInfo timelineInfo{};
    timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timelineInfo.pNext = NULL;
    timelineInfo.signalSemaphoreValueCount = 1;
    timelineInfo.pSignalSemaphoreValues = &signalValue;

    VkSubmitInfo submitInfoShadow{};
    submitInfoShadow.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfoShadow.pNext = &timelineInfo;
    VkSemaphore signalShadowSemaphores[] = {shadowSemaphore[currentFrame]->getSemaphore()};
    submitInfoShadow.signalSemaphoreCount = 1;
    submitInfoShadow.pSignalSemaphores = signalShadowSemaphores;
    submitInfoShadow.commandBufferCount = 1;
    submitInfoShadow.pCommandBuffers = &commandBuffer->getCommandBuffer()[currentFrame];

    // record command buffer
    commandBuffer->endCommands(
        currentFrame, 1, submitInfoShadow,
        shadowFences[lightManager->getDirectionalLights().size() + index * 6 + face][currentFrame]);
  } else {
    commandBuffer->endCommands(
        currentFrame, 1, false,
        shadowFences[lightManager->getDirectionalLights().size() + index * 6 + face][currentFrame]);
  }
}

void initialize() {
  settings = std::make_shared<Settings>();
  settings->setName("Vulkan");
  settings->setResolution(std::tuple{1600, 900});
  settings->setGraphicColorFormat(VK_FORMAT_R32G32B32A32_SFLOAT);
  settings->setSwapchainColorFormat(VK_FORMAT_B8G8R8A8_UNORM);
  settings->setLoadTextureColorFormat(VK_FORMAT_R8G8B8A8_SRGB);
  settings->setLoadTextureAuxilaryFormat(VK_FORMAT_R8G8B8A8_UNORM);

  settings->setDepthFormat(VK_FORMAT_D32_SFLOAT);
  settings->setMaxFramesInFlight(2);
  settings->setThreadsInPool(6);

  state = std::make_shared<State>(settings);
  window = state->getWindow();
  input = state->getInput();
  instance = state->getInstance();
  surface = state->getSurface();
  device = state->getDevice();
  swapchain = state->getSwapchain();
  descriptorPool = state->getDescriptorPool();

  auto nullHandle = vkGetDeviceProcAddr(state->getDevice()->getLogicalDevice(),
                                        "VkPhysicalDeviceRobustness2FeaturesEXT");

  // allocate commands
  commandPool = std::make_shared<CommandPool>(QueueType::GRAPHIC, device);
  commandBuffer = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(), commandPool, device);
  commandPoolTransfer = std::make_shared<CommandPool>(QueueType::TRANSFER, device);
  commandBufferTransfer = std::make_shared<CommandBuffer>(1, commandPool, device);
  commandPoolParticleSystem = std::make_shared<CommandPool>(QueueType::COMPUTE, device);
  commandBufferParticleSystem = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(),
                                                                commandPoolParticleSystem, device);
  commandPoolPostprocessing = std::make_shared<CommandPool>(QueueType::COMPUTE, device);
  commandBufferPostprocessing = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(),
                                                                commandPoolPostprocessing, device);
  commandPoolGUI = std::make_shared<CommandPool>(QueueType::GRAPHIC, device);
  commandBufferGUI = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(), commandPoolGUI, device);
  //
  loggerGPU = std::make_shared<LoggerGPU>(state);
  loggerCompute = std::make_shared<LoggerGPU>(state);
  loggerCPU = std::make_shared<LoggerCPU>();

  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    // graphic-presentation
    imageAvailableSemaphores.push_back(std::make_shared<Semaphore>(VK_SEMAPHORE_TYPE_BINARY, device));
    renderFinishedSemaphores.push_back(std::make_shared<Semaphore>(VK_SEMAPHORE_TYPE_BINARY, device));
    inFlightFences.push_back(std::make_shared<Fence>(device));
    // compute-graphic
    particleSystemSemaphore.push_back(std::make_shared<Semaphore>(VK_SEMAPHORE_TYPE_BINARY, device));
    particleSystemFences.push_back(std::make_shared<Fence>(device));
    semaphorePostprocessing.push_back(std::make_shared<Semaphore>(VK_SEMAPHORE_TYPE_BINARY, device));
    semaphoreGUI.push_back(std::make_shared<Semaphore>(VK_SEMAPHORE_TYPE_BINARY, device));
  }

  graphicTexture.resize(settings->getMaxFramesInFlight());
  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    auto graphicImage = std::make_shared<Image>(settings->getResolution(), 1, 1, settings->getGraphicColorFormat(),
                                                VK_IMAGE_TILING_OPTIMAL,
                                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->getDevice());
    graphicImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                               commandBufferTransfer);
    auto graphicImageView = std::make_shared<ImageView>(graphicImage, VK_IMAGE_VIEW_TYPE_2D, 1, 0, 1,
                                                        VK_IMAGE_ASPECT_COLOR_BIT, state->getDevice());
    graphicTexture[i] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, graphicImageView,
                                                  state->getDevice());
  }

  gui = std::make_shared<GUI>(settings, window, device);
  gui->initialize(commandBufferTransfer);

  auto texture = std::make_shared<Texture>("../data/brickwall.jpg", settings->getLoadTextureColorFormat(),
                                           VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer, device);
  auto normalMap = std::make_shared<Texture>("../data/brickwall_normal.jpg", settings->getLoadTextureAuxilaryFormat(),
                                             VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer, device);
  camera = std::make_shared<CameraFly>(settings);
  camera->setProjectionParameters(60.f, 0.1f, 100.f);
  input->subscribe(std::dynamic_pointer_cast<InputSubscriber>(camera));
  input->subscribe(std::dynamic_pointer_cast<InputSubscriber>(gui));
  lightManager = std::make_shared<LightManager>(commandBufferTransfer, state);
  pointLightHorizontal = lightManager->createPointLight(settings->getDepthResolution());
  pointLightHorizontal->createPhong(0.f, 0.f, glm::vec3(1.f, 1.f, 1.f));
  pointLightHorizontal->setPosition({3.f, 4.f, 0.f});
  /*pointLightVertical = lightManager->createPointLight(settings->getDepthResolution());
  pointLightVertical->createPhong(0.f, 1.f, glm::vec3(1.f, 1.f, 1.f));
  pointLightVertical->setPosition({-3.f, 4.f, 0.f});

  pointLightHorizontal2 = lightManager->createPointLight(settings->getDepthResolution());
  pointLightHorizontal2->createPhong(0.f, 1.f, glm::vec3(1.f, 1.f, 1.f));
  pointLightHorizontal2->setPosition({3.f, 4.f, 3.f});

  pointLightVertical2 = lightManager->createPointLight(settings->getDepthResolution());
  pointLightVertical2->createPhong(0.f, 1.f, glm::vec3(1.f, 1.f, 1.f));
  pointLightVertical2->setPosition({-3.f, 4.f, -3.f});*/

  directionalLight = lightManager->createDirectionalLight(settings->getDepthResolution());
  directionalLight->createPhong(0.2f, 0.f, glm::vec3(0.5f, 0.5f, 0.5f));
  directionalLight->setPosition({0.f, 15.f, 0.f});
  directionalLight->setCenter({0.f, 0.f, 0.f});
  directionalLight->setUp({0.f, 0.f, -1.f});

  /*directionalLight2 = lightManager->createDirectionalLight(settings->getDepthResolution());
  directionalLight2->createPhong(0.f, 1.f, glm::vec3(1.f, 1.f, 1.f));
  directionalLight2->setPosition({3.f, 15.f, 0.f});
  directionalLight2->setCenter({0.f, 0.f, 0.f});
  directionalLight2->setUp({0.f, 1.f, 0.f});*/

  spriteManager = std::make_shared<SpriteManager>(settings->getGraphicColorFormat(), lightManager,
                                                  commandBufferTransfer, descriptorPool, device, settings);
  modelManager = std::make_shared<Model3DManager>(settings->getGraphicColorFormat(), lightManager,
                                                  commandBufferTransfer, descriptorPool, device, settings);
  debugVisualization = std::make_shared<DebugVisualization>(camera, gui, commandBufferTransfer, state);
  debugVisualization->setLights(lightManager);
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

    // spriteManager->registerSprite(sprite);
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
    glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(-2.f, 0.f, -3.f));
    // model = glm::rotate(model, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
    // model = glm::scale(model, glm::vec3(20.f, 20.f, 20.f));
    modelGLTF->setModel(model);
  }

  modelManager->registerModelGLTF(modelGLTF);

  for (int i = 0; i < lightManager->getDirectionalLights().size(); i++) {
    auto commandPool = std::make_shared<CommandPool>(QueueType::GRAPHIC, state->getDevice());
    commandPoolDirectional.push_back(commandPool);
    commandBufferDirectional.push_back(
        std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(), commandPool, state->getDevice()));
    loggerGPUDirectional.push_back(std::make_shared<LoggerGPU>(state));
  }

  commandPoolPoint.resize(lightManager->getPointLights().size());
  commandBufferPoint.resize(lightManager->getPointLights().size());
  loggerGPUPoint.resize(lightManager->getPointLights().size());
  for (int i = 0; i < lightManager->getPointLights().size(); i++) {
    for (int j = 0; j < 6; j++) {
      auto commandPool = std::make_shared<CommandPool>(QueueType::GRAPHIC, state->getDevice());
      commandPoolPoint[i].push_back(commandPool);
      commandBufferPoint[i].push_back(
          std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(), commandPool, state->getDevice()));
      loggerGPUPoint[i].push_back(std::make_shared<LoggerGPU>(state));
    }
  }

  terrain = std::make_shared<TerrainGPU>(std::pair{12, 12}, settings->getGraphicColorFormat(), commandBufferTransfer,
                                         lightManager, state);
  {
    auto scaleMatrix = glm::scale(glm::mat4(1.f), glm::vec3(0.1f, 0.1f, 0.1f));
    auto translateMatrix = glm::translate(scaleMatrix, glm::vec3(2.f, -6.f, 0.f));
    terrain->setModel(translateMatrix);
  }
  terrain->setCamera(camera);

  auto particleTexture = std::make_shared<Texture>("../data/Particles/gradient.png",
                                                   settings->getLoadTextureAuxilaryFormat(),
                                                   VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer, device);
  particleSystem = std::make_shared<ParticleSystem>(300, state->getSettings()->getGraphicColorFormat(), particleTexture,
                                                    commandBufferTransfer, state);
  {
    auto matrix = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, 2.f));
    matrix = glm::scale(matrix, glm::vec3(0.5f, 0.5f, 0.5f));

    particleSystem->setModel(matrix);
  }
  particleSystem->setCamera(camera);

  swapchain->changeImageLayout(VK_IMAGE_LAYOUT_GENERAL, commandBufferTransfer);
  postprocessing = std::make_shared<Postprocessing>(graphicTexture, swapchain->getImageViews(), state);

  debugVisualization->setPostprocessing(postprocessing);

  shadowSemaphore.resize(settings->getMaxFramesInFlight());
  particleSemaphore.resize(settings->getMaxFramesInFlight());
  for (int i = 0; i < shadowSemaphore.size(); i++) {
    shadowSemaphore[i] = std::make_shared<Semaphore>(VK_SEMAPHORE_TYPE_TIMELINE, device);
    particleSemaphore[i] = std::make_shared<Semaphore>(VK_SEMAPHORE_TYPE_TIMELINE, device);
  }

  shadowFences.resize(lightManager->getDirectionalLights().size() + lightManager->getPointLights().size() * 6);
  for (int i = 0; i < lightManager->getDirectionalLights().size() + lightManager->getPointLights().size() * 6; i++) {
    shadowFences[i].resize(settings->getMaxFramesInFlight());
    for (int j = 0; j < settings->getMaxFramesInFlight(); j++) {
      shadowFences[i][j] = std::make_shared<Fence>(device);
    }
  }

  pool = std::make_shared<BS::thread_pool>(6);
  pool2 = std::make_shared<BS::thread_pool>(1);
}

void drawFrame() {
  //***************************************************************************************************************
  // process particle system
  std::vector<VkFence> waitParticleSystemFence = {particleSystemFences[currentFrame]->getFence()};
  auto result = vkWaitForFences(device->getLogicalDevice(), waitParticleSystemFence.size(),
                                waitParticleSystemFence.data(), VK_TRUE, UINT64_MAX);
  result = vkResetFences(device->getLogicalDevice(), waitParticleSystemFence.size(), waitParticleSystemFence.data());
  if (result != VK_SUCCESS) throw std::runtime_error("Can't reset fence");

  // result = vkResetCommandBuffer(commandBufferParticleSystem->getCommandBuffer()[currentFrame],
  //                               /*VkCommandBufferResetFlagBits*/ 0);
  // if (result != VK_SUCCESS) throw std::runtime_error("Can't reset cmd buffer");

  commandBufferParticleSystem->beginCommands(currentFrame);
  loggerCompute->setCommandBufferName("Particles compute command buffer", currentFrame, commandBufferParticleSystem);

  loggerCompute->begin("Particle system compute " + std::to_string(globalFrame), currentFrame);
  particleSystem->drawCompute(currentFrame, commandBufferParticleSystem);
  loggerCompute->end(currentFrame);

  particleSystem->updateTimer(frameTimer);

  uint64_t particleSignal = globalFrame + 1;
  VkTimelineSemaphoreSubmitInfo timelineInfo{};
  timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
  timelineInfo.pNext = NULL;
  timelineInfo.signalSemaphoreValueCount = 1;
  timelineInfo.pSignalSemaphoreValues = &particleSignal;

  VkSubmitInfo submitInfoCompute{};
  submitInfoCompute.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfoCompute.pNext = &timelineInfo;
  VkSemaphore signalParticleSemaphores[] = {particleSemaphore[currentFrame]->getSemaphore()};
  submitInfoCompute.signalSemaphoreCount = 1;
  submitInfoCompute.pSignalSemaphores = signalParticleSemaphores;
  submitInfoCompute.commandBufferCount = 1;
  submitInfoCompute.pCommandBuffers = &commandBufferParticleSystem->getCommandBuffer()[currentFrame];

  // end command buffer
  commandBufferParticleSystem->endCommands(currentFrame, 0, submitInfoCompute, particleSystemFences[currentFrame]);

  //***************************************************************************************************************
  // process shadows
  std::vector<VkFence> waitFences = {inFlightFences[currentFrame]->getFence()};
  result = vkWaitForFences(device->getLogicalDevice(), waitFences.size(), waitFences.data(), VK_TRUE, UINT64_MAX);
  if (result != VK_SUCCESS) throw std::runtime_error("Can't wait for fence");

  shadowIndex[currentFrame] = 0;
  shadowSemaphore[currentFrame]->reset();

  uint32_t imageIndex;
  // RETURNS ONLY INDEX, NOT IMAGE
  // semaphore to signal, once image is available
  result = vkAcquireNextImageKHR(device->getLogicalDevice(), swapchain->getSwapchain(), UINT64_MAX,
                                 imageAvailableSemaphores[currentFrame]->getSemaphore(), VK_NULL_HANDLE, &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    // TODO: recreate swapchain
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

  result = vkResetFences(device->getLogicalDevice(), waitFences.size(), waitFences.data());
  if (result != VK_SUCCESS) throw std::runtime_error("Can't reset fence");

  // result = vkResetCommandBuffer(commandBuffer->getCommandBuffer()[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
  // if (result != VK_SUCCESS) throw std::runtime_error("Can't reset cmd buffer");

  gui->drawText("FPS", {20, 20}, {std::to_string(fps)});

  // update positions
  static float angleHorizontal = 90.f;
  glm::vec3 lightPositionHorizontal = glm::vec3(15.f * cos(glm::radians(angleHorizontal)), 15.f,
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

  // TODO: find the better way this one is too long
  if (updateJoints.valid()) {
    updateJoints.get();
  }

  /////////////////////////////////////////////////////////////////////////////////////////
  // render to depth buffer
  /////////////////////////////////////////////////////////////////////////////////////////
  for (int i = 0; i < lightManager->getDirectionalLights().size(); i++) {
    pool->push_task(directionalLightCalculator, i);
  }

  for (int i = 0; i < lightManager->getPointLights().size(); i++) {
    for (int j = 0; j < 6; j++) {
      pool->push_task(pointLightCalculator, i, j);
    }
  }

  // record command buffer
  commandBuffer->beginCommands(currentFrame);
  loggerGPU->setCommandBufferName("Draw command buffer", currentFrame, commandBuffer);

  /////////////////////////////////////////////////////////////////////////////////////////
  // depth to screne barrier
  /////////////////////////////////////////////////////////////////////////////////////////
  // Image memory barrier to make sure that writes are finished before sampling from the texture

  /////////////////////////////////////////////////////////////////////////////////////////
  // render graphic
  /////////////////////////////////////////////////////////////////////////////////////////
  {
    VkClearValue clearColor;
    clearColor.color = settings->getClearColor();
    const VkRenderingAttachmentInfo colorAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = graphicTexture[currentFrame]->getImageView()->getImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clearColor,
    };

    VkClearValue clearDepth;
    clearDepth.depthStencil = {1.0f, 0};
    const VkRenderingAttachmentInfo depthAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchain->getDepthImageView()->getImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clearDepth,
    };

    auto [width, height] = graphicTexture[currentFrame]->getImageView()->getImage()->getResolution();
    VkRect2D renderArea{};
    renderArea.extent.width = width;
    renderArea.extent.height = height;
    const VkRenderingInfo renderInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = renderArea,
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentInfo,
        .pDepthAttachment = &depthAttachmentInfo,
    };

    vkCmdBeginRendering(commandBuffer->getCommandBuffer()[currentFrame], &renderInfo);
    loggerGPU->begin("Render light " + std::to_string(globalFrame), currentFrame);
    lightManager->draw(currentFrame);
    loggerGPU->end(currentFrame);
    // draw scene here
    loggerGPU->begin("Render sprites " + std::to_string(globalFrame), currentFrame);
    spriteManager->draw(currentFrame, commandBuffer);
    loggerGPU->end(currentFrame);

    pool2->wait_for_tasks();
    loggerGPU->begin("Render models " + std::to_string(globalFrame), currentFrame);
    modelManager->draw(currentFrame, commandBuffer);
    loggerGPU->end(currentFrame);
    updateJoints = pool2->submit([&]() {
      loggerCPU->begin("Update animation " + std::to_string(globalFrame));
      modelManager->updateAnimation(currentFrame, frameTimer);
      loggerCPU->end();
    });

    loggerGPU->begin("Render terrain " + std::to_string(globalFrame), currentFrame);
    {
      std::map<std::string, bool*> terrainGUI;
      terrainGUI["Normals"] = &terrainNormals;
      terrainGUI["Wireframe"] = &terrainWireframe;
      gui->drawCheckbox("Terrain", {std::get<0>(settings->getResolution()) - 160, 350}, terrainGUI);
    }
    {
      std::map<std::string, bool*> terrainGUI;
      terrainGUI["Patches"] = &terrainPatch;
      if (gui->drawCheckbox("Terrain", {std::get<0>(settings->getResolution()) - 160, 350}, terrainGUI))
        std::dynamic_pointer_cast<TerrainGPU>(terrain)->patchEdge(terrainPatch);
    }
    {
      std::map<std::string, bool*> terrainGUI;
      terrainGUI["LoD"] = &showLoD;
      if (gui->drawCheckbox("Terrain", {std::get<0>(settings->getResolution()) - 160, 350}, terrainGUI))
        std::dynamic_pointer_cast<TerrainGPU>(terrain)->showLoD(showLoD);
    }
    if (terrainWireframe)
      terrain->draw(currentFrame, commandBuffer, TerrainPipeline::WIREFRAME);
    else
      terrain->draw(currentFrame, commandBuffer, TerrainPipeline::FILL);
    if (terrainNormals) terrain->draw(currentFrame, commandBuffer, TerrainPipeline::NORMAL);
    loggerGPU->end(currentFrame);

    // contains transparency, should be drawn last
    loggerGPU->begin("Render particles " + std::to_string(globalFrame), currentFrame);
    particleSystem->drawGraphic(currentFrame, commandBuffer);
    loggerGPU->end(currentFrame);

    vkCmdEndRendering(commandBuffer->getCommandBuffer()[currentFrame]);

    std::vector<VkSemaphore> waitSemaphores = {particleSemaphore[currentFrame]->getSemaphore(),
                                               shadowSemaphore[currentFrame]->getSemaphore()};

    uint64_t shadowWait = (lightManager->getDirectionalLights().size() + lightManager->getPointLights().size() * 6);
    std::vector<uint64_t> waitSemaphoreValues = {particleSignal, shadowWait};
    VkTimelineSemaphoreSubmitInfo waitShadowTimeline{};
    waitShadowTimeline.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    waitShadowTimeline.pNext = NULL;
    waitShadowTimeline.waitSemaphoreValueCount = waitSemaphores.size();
    waitShadowTimeline.pWaitSemaphoreValues = waitSemaphoreValues.data();

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = &waitShadowTimeline;

    std::vector<VkPipelineStageFlags> waitStages = {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                                                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};

    submitInfo.waitSemaphoreCount = waitSemaphores.size();
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer->getCommandBuffer()[currentFrame];

    VkSemaphore signalSemaphores[] = {semaphorePostprocessing[currentFrame]->getSemaphore()};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    commandBuffer->endCommands(currentFrame, 0, submitInfo, nullptr);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Render compute postprocessing
  //////////////////////////////////////////////////////////////////////////////////////////////////
  {
    commandBufferPostprocessing->beginCommands(currentFrame);
    loggerCompute->setCommandBufferName("Postprocessing command buffer", currentFrame, commandBufferPostprocessing);
    if (layoutChanged.contains(imageIndex) && layoutChanged[imageIndex] == true) {
      // Change layout from COLOR_ATTACHMENT to PRESENT_SRC_KHR
      VkImageMemoryBarrier colorBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                        .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                        .image = swapchain->getImageViews()[imageIndex]->getImage()->getImage(),
                                        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                             .baseMipLevel = 0,
                                                             .levelCount = 1,
                                                             .baseArrayLayer = 0,
                                                             .layerCount = 1}};
      vkCmdPipelineBarrier(commandBufferPostprocessing->getCommandBuffer()[currentFrame],
                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,     // srcStageMask
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // dstStageMask
                           0, 0, nullptr, 0, nullptr,
                           1,             // imageMemoryBarrierCount
                           &colorBarrier  // pImageMemoryBarriers
      );
    }
    loggerCompute->begin("Postprocessing compute " + std::to_string(globalFrame), currentFrame);
    postprocessing->drawCompute(currentFrame, imageIndex, commandBufferPostprocessing);
    loggerCompute->end(currentFrame);

    pool->wait_for_tasks();

    VkSubmitInfo submitInfoPostprocessing{};
    submitInfoPostprocessing.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    std::vector<VkSemaphore> waitSemaphores = {imageAvailableSemaphores[currentFrame]->getSemaphore(),
                                               semaphorePostprocessing[currentFrame]->getSemaphore()};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
    submitInfoPostprocessing.waitSemaphoreCount = waitSemaphores.size();
    submitInfoPostprocessing.pWaitSemaphores = waitSemaphores.data();
    submitInfoPostprocessing.pWaitDstStageMask = waitStages;

    VkSemaphore signalComputeSemaphores[] = {semaphoreGUI[currentFrame]->getSemaphore()};
    submitInfoPostprocessing.signalSemaphoreCount = 1;
    submitInfoPostprocessing.pSignalSemaphores = signalComputeSemaphores;
    submitInfoPostprocessing.commandBufferCount = 1;
    submitInfoPostprocessing.pCommandBuffers = &commandBufferPostprocessing->getCommandBuffer()[currentFrame];

    // end command buffer
    commandBufferPostprocessing->endCommands(currentFrame, 0, submitInfoPostprocessing, nullptr);
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Render debug visualization
  //////////////////////////////////////////////////////////////////////////////////////////////////
  {
    commandBufferGUI->beginCommands(currentFrame);
    loggerGPU->setCommandBufferName("GUI command buffer", currentFrame, commandBufferGUI);

    VkClearValue clearColor;
    clearColor.color = settings->getClearColor();
    const VkRenderingAttachmentInfo colorAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchain->getImageViews()[imageIndex]->getImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clearColor,
    };

    const VkRenderingAttachmentInfo depthAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchain->getDepthImageView()->getImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };

    auto [width, height] = swapchain->getImageViews()[imageIndex]->getImage()->getResolution();
    VkRect2D renderArea{};
    renderArea.extent.width = width;
    renderArea.extent.height = height;
    const VkRenderingInfo renderInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = renderArea,
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentInfo,
        .pDepthAttachment = &depthAttachmentInfo,
    };

    vkCmdBeginRendering(commandBufferGUI->getCommandBuffer()[currentFrame], &renderInfo);
    loggerGPU->begin("Render debug visualization " + std::to_string(globalFrame), currentFrame);
    debugVisualization->draw(currentFrame, commandBufferGUI);
    loggerGPU->end(currentFrame);

    loggerGPU->begin("Render GUI " + std::to_string(globalFrame), currentFrame);
    gui->updateBuffers(currentFrame);
    gui->drawFrame(currentFrame, commandBufferGUI);
    loggerGPU->end(currentFrame);

    vkCmdEndRendering(commandBufferGUI->getCommandBuffer()[currentFrame]);

    // Change layout from COLOR_ATTACHMENT to PRESENT_SRC_KHR
    VkImageMemoryBarrier colorBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                      .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                                      .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                                      .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                      .image = swapchain->getImageViews()[imageIndex]->getImage()->getImage(),
                                      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                           .baseMipLevel = 0,
                                                           .levelCount = 1,
                                                           .baseArrayLayer = 0,
                                                           .layerCount = 1}};
    vkCmdPipelineBarrier(commandBufferGUI->getCommandBuffer()[currentFrame],
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // srcStageMask
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,  // dstStageMask
                         0, 0, nullptr, 0, nullptr,
                         1,             // imageMemoryBarrierCount
                         &colorBarrier  // pImageMemoryBarriers
    );

    layoutChanged[imageIndex] = true;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    std::vector<VkSemaphore> waitSemaphores = {semaphoreGUI[currentFrame]->getSemaphore()};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};
    submitInfo.waitSemaphoreCount = waitSemaphores.size();
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBufferGUI->getCommandBuffer()[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]->getSemaphore()};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    // end command buffer
    // to render, need to wait semaphore from vkAcquireNextImageKHR, so display surface is ready
    // signal inFlightFences once all commands on GPU finish execution
    commandBufferGUI->endCommands(currentFrame, 0, submitInfo, inFlightFences[currentFrame]);
  }

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  std::vector<VkSemaphore> waitSemaphoresPresent = {renderFinishedSemaphores[currentFrame]->getSemaphore()};

  presentInfo.waitSemaphoreCount = waitSemaphoresPresent.size();
  presentInfo.pWaitSemaphores = waitSemaphoresPresent.data();

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
  globalFrame += 1;
  currentFrame = globalFrame % settings->getMaxFramesInFlight();
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
  shouldWork = false;
}

int main() {
#ifdef WIN32
  SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
#endif
  try {
    initialize();
    mainLoop();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}