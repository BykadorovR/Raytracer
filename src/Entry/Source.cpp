#include <iostream>
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
#include "Blur.h"
#include "Loader.h"
#include "Cube.h"
#include "Skybox.h"
#include "Cubemap.h"
#include "Equirectangular.h"
#include "Timer.h"

std::shared_ptr<Timer> timer = std::make_shared<Timer>();
std::shared_ptr<TimerFPS> timerFPSReal = std::make_shared<TimerFPS>();
std::shared_ptr<TimerFPS> timerFPSLimited = std::make_shared<TimerFPS>();
int currentFrame = 0;
// Depth bias (and slope) are used to avoid shadowing artifacts
// Constant depth bias factor (always applied)
float depthBiasConstant = 1.25f;
// Slope depth bias factor, applied depending on polygon's slope
float depthBiasSlope = 1.75f;

std::shared_ptr<Window> window;
std::shared_ptr<Instance> instance;
std::shared_ptr<Device> device;
std::shared_ptr<CommandBuffer> commandBufferEquirectangular, commandBufferRender, commandBufferTransfer,
    commandBufferParticleSystem, commandBufferPostprocessing, commandBufferBlur, commandBufferGUI;
std::shared_ptr<CommandPool> commandPoolEquirectangular, commandPoolRender, commandPoolTransfer,
    commandPoolParticleSystem, commandPoolPostprocessing, commandPoolGUI;
std::shared_ptr<DescriptorPool> descriptorPool;
std::shared_ptr<Surface> surface;
std::shared_ptr<Settings> settings;
std::shared_ptr<Terrain> terrain;
std::shared_ptr<State> state;

std::vector<std::shared_ptr<Semaphore>> imageAvailableSemaphores, renderFinishedSemaphores, particleSystemSemaphore,
    semaphorePostprocessing, semaphoreGUI;
std::vector<std::shared_ptr<Fence>> inFlightFences, particleSystemFences;

std::shared_ptr<SpriteManager> spriteManager, spriteManagerHUD;
std::shared_ptr<Model3DManager> modelManager;
std::shared_ptr<ParticleSystem> particleSystem;
std::shared_ptr<Postprocessing> postprocessing;
std::shared_ptr<Blur> blur;
std::shared_ptr<Input> input;
std::shared_ptr<GUI> gui;
std::shared_ptr<Swapchain> swapchain;
std::shared_ptr<CameraFly> camera;
std::shared_ptr<CameraOrtho> cameraOrtho;
std::shared_ptr<LightManager> lightManager;
std::shared_ptr<PointLight> pointLightHorizontal, pointLightVertical, pointLightHorizontal2, pointLightVertical2;
std::shared_ptr<DirectionalLight> directionalLight, directionalLight2;
std::shared_ptr<AmbientLight> ambientLight;
std::shared_ptr<DebugVisualization> debugVisualization;
std::shared_ptr<LoggerGPU> loggerGPU, loggerPostprocessing, loggerParticles, loggerGPUDebug;
std::shared_ptr<LoggerCPU> loggerCPU;

std::future<void> updateJoints;
std::shared_ptr<Animation> animation;

std::vector<std::shared_ptr<Texture>> graphicTexture, blurTextureIn, blurTextureOut;
std::shared_ptr<Cubemap> cubemapEquirectangular, cubemapDiffuse, cubemapSpecular;

std::shared_ptr<Cube> equiCube, diffuseCube, specularCube;

std::shared_ptr<BS::thread_pool> pool, pool2;
std::vector<std::shared_ptr<CommandPool>> commandPoolDirectional;
std::vector<std::vector<std::shared_ptr<CommandPool>>> commandPoolPoint;

std::vector<std::shared_ptr<CommandBuffer>> commandBufferDirectional;
std::vector<std::vector<std::shared_ptr<CommandBuffer>>> commandBufferPoint;

std::vector<std::shared_ptr<LoggerGPU>> loggerGPUDirectional;
std::vector<std::vector<std::shared_ptr<LoggerGPU>>> loggerGPUPoint;
bool terrainWireframe = false;
bool terrainPatch = false;
bool showLoD = false;
bool shouldWork = true;
std::map<int, bool> layoutChanged;
std::mutex debugVisualizationMutex;
uint64_t particleSignal;
bool FPSChanged = false;

std::vector<std::shared_ptr<Sphere>> spheres;
std::shared_ptr<Cubemap> cubemap;
std::shared_ptr<Cube> cube;
std::shared_ptr<Skybox> skybox;

std::shared_ptr<Cube> cubeTemp;
std::shared_ptr<Equirectangular> equirectangular;
std::shared_ptr<MaterialColor> materialColorDiffuse;

void directionalLightCalculator(int index) {
  auto commandBuffer = commandBufferDirectional[index];
  auto loggerGPU = loggerGPUDirectional[index];

  // record command buffer
  commandBuffer->beginCommands(currentFrame);
  loggerGPU->setCommandBufferName("Directional command buffer", currentFrame, commandBuffer);

  loggerGPU->begin("Directional to depth buffer " + std::to_string(timer->getFrameCounter()), currentFrame);
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
  auto globalFrame = timer->getFrameCounter();
  loggerGPU->begin("Sprites to directional depth buffer " + std::to_string(globalFrame), currentFrame);
  spriteManager->drawShadow(currentFrame, commandBuffer, LightType::DIRECTIONAL, index);
  loggerGPU->end();
  loggerGPU->begin("Models to directional depth buffer " + std::to_string(globalFrame), currentFrame);
  modelManager->drawShadow(currentFrame, commandBuffer, LightType::DIRECTIONAL, index);
  loggerGPU->end();
  loggerGPU->begin("Terrain to directional depth buffer " + std::to_string(globalFrame), currentFrame);
  terrain->drawShadow(currentFrame, commandBuffer, LightType::DIRECTIONAL, index);
  loggerGPU->end();
  loggerGPU->begin("Cube to directional depth buffer " + std::to_string(globalFrame), currentFrame);
  cube->drawShadow(currentFrame, commandBuffer, LightType::DIRECTIONAL, index);
  loggerGPU->end();
  vkCmdEndRendering(commandBuffer->getCommandBuffer()[currentFrame]);
  loggerGPU->end();

  // record command buffer
  commandBuffer->endCommands();
  commandBuffer->submitToQueue(false);
}

void pointLightCalculator(int index, int face) {
  auto commandBuffer = commandBufferPoint[index][face];
  auto loggerGPU = loggerGPUPoint[index][face];
  // record command buffer
  loggerGPU->setCommandBufferName("Point " + std::to_string(index) + "x" + std::to_string(face) + " command buffer",
                                  currentFrame, commandBuffer);
  commandBuffer->beginCommands(currentFrame);
  loggerGPU->begin("Point to depth buffer " + std::to_string(timer->getFrameCounter()), currentFrame);
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
                       ->getTextureSeparate()[face][0]
                       ->getImageView()
                       ->getImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearDepth,
  };

  auto [width, height] = pointLights[index]
                             ->getDepthCubemap()[currentFrame]
                             ->getTextureSeparate()[face][0]
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
  auto globalFrame = timer->getFrameCounter();
  float aspect = std::get<0>(settings->getResolution()) / std::get<1>(settings->getResolution());
  loggerGPU->begin("Sprites to point depth buffer " + std::to_string(globalFrame), currentFrame);
  spriteManager->drawShadow(currentFrame, commandBuffer, LightType::POINT, index, face);
  loggerGPU->end();
  loggerGPU->begin("Models to point depth buffer " + std::to_string(globalFrame), currentFrame);
  modelManager->drawShadow(currentFrame, commandBuffer, LightType::POINT, index, face);
  loggerGPU->end();
  loggerGPU->begin("Terrain to point depth buffer " + std::to_string(globalFrame), currentFrame);
  terrain->drawShadow(currentFrame, commandBuffer, LightType::POINT, index, face);
  loggerGPU->end();
  loggerGPU->begin("Cube to point depth buffer " + std::to_string(globalFrame), currentFrame);
  cube->drawShadow(currentFrame, commandBuffer, LightType::POINT, index, face);
  loggerGPU->end();
  vkCmdEndRendering(commandBuffer->getCommandBuffer()[currentFrame]);
  loggerGPU->end();

  // record command buffer
  commandBuffer->endCommands();
  commandBuffer->submitToQueue(false);
}

void computeParticles() {
  commandBufferParticleSystem->beginCommands(currentFrame);
  loggerParticles->setCommandBufferName("Particles compute command buffer", currentFrame, commandBufferParticleSystem);

  // any read from SSBO should wait for write to SSBO
  // First dispatch writes to a storage buffer, second dispatch reads from that storage buffer.
  VkMemoryBarrier memoryBarrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT};
  vkCmdPipelineBarrier(commandBufferParticleSystem->getCommandBuffer()[currentFrame],
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // srcStageMask
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // dstStageMask
                       0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

  loggerParticles->begin("Particle system compute " + std::to_string(timer->getFrameCounter()), currentFrame);
  particleSystem->drawCompute(currentFrame, commandBufferParticleSystem);
  loggerParticles->end();

  particleSystem->updateTimer(timer->getElapsedCurrent());

  particleSignal = timer->getFrameCounter() + 1;
  VkTimelineSemaphoreSubmitInfo timelineInfo{};
  timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
  timelineInfo.pNext = NULL;
  timelineInfo.signalSemaphoreValueCount = 1;
  timelineInfo.pSignalSemaphoreValues = &particleSignal;

  VkSubmitInfo submitInfoCompute{};
  submitInfoCompute.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfoCompute.pNext = &timelineInfo;
  VkSemaphore signalParticleSemaphores[] = {particleSystemSemaphore[currentFrame]->getSemaphore()};
  submitInfoCompute.signalSemaphoreCount = 1;
  submitInfoCompute.pSignalSemaphores = signalParticleSemaphores;
  submitInfoCompute.commandBufferCount = 1;
  submitInfoCompute.pCommandBuffers = &commandBufferParticleSystem->getCommandBuffer()[currentFrame];

  // end command buffer
  commandBufferParticleSystem->endCommands();
  commandBufferParticleSystem->submitToQueue(submitInfoCompute, nullptr);
}

void computePostprocessing(int swapchainImageIndex) {
  commandBufferPostprocessing->beginCommands(currentFrame);
  loggerPostprocessing->setCommandBufferName("Postprocessing command buffer", currentFrame,
                                             commandBufferPostprocessing);
  int bloomPasses = settings->getBloomPasses();
  // blur cycle:
  // in - out - horizontal
  // out - in - vertical
  for (int i = 0; i < bloomPasses; i++) {
    loggerPostprocessing->begin("Blur horizontal compute " + std::to_string(timer->getFrameCounter()), currentFrame);
    blur->drawCompute(currentFrame, true, commandBufferPostprocessing);
    loggerPostprocessing->end();

    // sync between horizontal and vertical
    // wait dst (textureOut) to be written
    {
      VkImageMemoryBarrier colorBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                                        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                                        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                        .image = blurTextureOut[currentFrame]->getImageView()->getImage()->getImage(),
                                        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                             .baseMipLevel = 0,
                                                             .levelCount = 1,
                                                             .baseArrayLayer = 0,
                                                             .layerCount = 1}};
      vkCmdPipelineBarrier(commandBufferPostprocessing->getCommandBuffer()[currentFrame],
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // srcStageMask
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // dstStageMask
                           0, 0, nullptr, 0, nullptr,
                           1,             // imageMemoryBarrierCount
                           &colorBarrier  // pImageMemoryBarriers
      );
    }

    loggerPostprocessing->begin("Blur vertical compute " + std::to_string(timer->getFrameCounter()), currentFrame);
    blur->drawCompute(currentFrame, false, commandBufferPostprocessing);
    loggerPostprocessing->end();

    // wait blur image to be ready
    // blurTextureIn stores result after blur (vertical part, out - in)
    {
      VkImageMemoryBarrier colorBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                                        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                                        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                        .image = blurTextureIn[currentFrame]->getImageView()->getImage()->getImage(),
                                        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                             .baseMipLevel = 0,
                                                             .levelCount = 1,
                                                             .baseArrayLayer = 0,
                                                             .layerCount = 1}};
      vkCmdPipelineBarrier(commandBufferPostprocessing->getCommandBuffer()[currentFrame],
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // srcStageMask
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // dstStageMask
                           0, 0, nullptr, 0, nullptr,
                           1,             // imageMemoryBarrierCount
                           &colorBarrier  // pImageMemoryBarriers
      );
    }
  }
  // wait dst image to be ready
  {
    VkImageMemoryBarrier colorBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                      .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                      .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                      .image = swapchain->getImageViews()[swapchainImageIndex]->getImage()->getImage(),
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

  loggerPostprocessing->begin("Postprocessing compute " + std::to_string(timer->getFrameCounter()), currentFrame);
  postprocessing->drawCompute(currentFrame, swapchainImageIndex, commandBufferPostprocessing);
  loggerPostprocessing->end();
  commandBufferPostprocessing->endCommands();
}

void debugVisualizations(int swapchainImageIndex) {
  commandBufferGUI->beginCommands(currentFrame);
  loggerGPUDebug->setCommandBufferName("GUI command buffer", currentFrame, commandBufferGUI);

  VkClearValue clearColor;
  clearColor.color = settings->getClearColor();
  const VkRenderingAttachmentInfo colorAttachmentInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = swapchain->getImageViews()[swapchainImageIndex]->getImageView(),
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

  auto [width, height] = swapchain->getImageViews()[swapchainImageIndex]->getImage()->getResolution();
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

  auto globalFrame = timer->getFrameCounter();
  loggerGPUDebug->begin("Calculate debug visualization " + std::to_string(globalFrame), currentFrame);
  debugVisualization->calculate(commandBufferGUI);
  loggerGPUDebug->end();

  vkCmdBeginRendering(commandBufferGUI->getCommandBuffer()[currentFrame], &renderInfo);
  loggerGPUDebug->begin("Render debug visualization " + std::to_string(globalFrame), currentFrame);
  debugVisualization->draw(currentFrame, commandBufferGUI);
  loggerGPUDebug->end();

  loggerGPUDebug->begin("Render GUI " + std::to_string(globalFrame), currentFrame);
  // TODO: move to debug visualization
  int blurKernelSize = blur->getKernelSize();
  if (gui->drawInputInt("Bloom", {20, 500}, {{"Kernel", &blurKernelSize}})) {
    blur->setKernelSize(blurKernelSize);
  }
  int blurSigma = blur->getSigma();
  if (gui->drawInputInt("Bloom", {20, 500}, {{"Sigma", &blurSigma}})) {
    blur->setSigma(blurSigma);
  }
  int bloomPasses = settings->getBloomPasses();
  if (gui->drawInputInt("Bloom", {20, 500}, {{"Passes", &bloomPasses}})) {
    settings->setBloomPasses(bloomPasses);
  }

  int desiredFPS = settings->getDesiredFPS();
  gui->drawText("FPS", {20, 20},
                {std::to_string(timerFPSLimited->getFPS()) + "/" + std::to_string(timerFPSReal->getFPS())});
  if (gui->drawInputInt("FPS", {20, 20}, {{"##current", &desiredFPS}})) {
    settings->setDesiredFPS(desiredFPS);
    FPSChanged = true;
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
  gui->updateBuffers(currentFrame);
  gui->drawFrame(currentFrame, commandBufferGUI);
  loggerGPUDebug->end();

  vkCmdEndRendering(commandBufferGUI->getCommandBuffer()[currentFrame]);

  VkImageMemoryBarrier colorBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                                    .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                                    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                    .image = swapchain->getImageViews()[swapchainImageIndex]->getImage()->getImage(),
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

  commandBufferGUI->endCommands();
}

// TODO: pass appropriate command buffer here as in lighting
void renderGraphic() {
  // record command buffer
  commandBufferRender->beginCommands(currentFrame);
  loggerGPU->setCommandBufferName("Draw command buffer", currentFrame, commandBufferRender);

  /////////////////////////////////////////////////////////////////////////////////////////
  // depth to screne barrier
  /////////////////////////////////////////////////////////////////////////////////////////
  // Image memory barrier to make sure that writes are finished before sampling from the texture
  int directionalNum = lightManager->getDirectionalLights().size();
  int pointNum = lightManager->getPointLights().size();
  std::vector<VkImageMemoryBarrier> imageMemoryBarrier(directionalNum + pointNum);
  for (int i = 0; i < directionalNum; i++) {
    imageMemoryBarrier[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // We won't be changing the layout of the image
    imageMemoryBarrier[i].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
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
    int id = directionalNum + i;
    imageMemoryBarrier[id].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // We won't be changing the layout of the image
    imageMemoryBarrier[id].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    imageMemoryBarrier[id].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    imageMemoryBarrier[id].image = lightManager->getPointLights()[i]
                                       ->getDepthCubemap()[currentFrame]
                                       ->getTexture()
                                       ->getImageView()
                                       ->getImage()
                                       ->getImage();
    imageMemoryBarrier[id].subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    imageMemoryBarrier[id].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    imageMemoryBarrier[id].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imageMemoryBarrier[id].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier[id].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  }
  vkCmdPipelineBarrier(commandBufferRender->getCommandBuffer()[currentFrame],
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, imageMemoryBarrier.size(),
                       imageMemoryBarrier.data());

  /////////////////////////////////////////////////////////////////////////////////////////
  // render graphic
  /////////////////////////////////////////////////////////////////////////////////////////
  VkClearValue clearColor;
  clearColor.color = settings->getClearColor();
  std::vector<VkRenderingAttachmentInfo> colorAttachmentInfo(2);
  // here we render scene as is
  colorAttachmentInfo[0] = VkRenderingAttachmentInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = graphicTexture[currentFrame]->getImageView()->getImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearColor,
  };
  // here we render bloom that will be applied on postprocessing stage to simple render
  colorAttachmentInfo[1] = VkRenderingAttachmentInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = blurTextureIn[currentFrame]->getImageView()->getImageView(),
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
      .colorAttachmentCount = (uint32_t)colorAttachmentInfo.size(),
      .pColorAttachments = colorAttachmentInfo.data(),
      .pDepthAttachment = &depthAttachmentInfo,
  };

  auto globalFrame = timer->getFrameCounter();
  vkCmdBeginRendering(commandBufferRender->getCommandBuffer()[currentFrame], &renderInfo);
  loggerGPU->begin("Render light " + std::to_string(globalFrame), currentFrame);
  lightManager->draw(currentFrame);
  loggerGPU->end();

  // draw scene here
  loggerGPU->begin("Render sprites " + std::to_string(globalFrame), currentFrame);
  spriteManager->draw(currentFrame, settings->getResolution(), commandBufferRender, settings->getDrawType());
  loggerGPU->end();

  // wait model3D update
  if (updateJoints.valid()) {
    updateJoints.get();
  }

  loggerGPU->begin("Render models " + std::to_string(globalFrame), currentFrame);
  modelManager->draw(currentFrame, commandBufferRender, settings->getDrawType());
  loggerGPU->end();

  // submit model3D update
  updateJoints = pool->submit([&]() {
    loggerCPU->begin("Update animation " + std::to_string(globalFrame));
    // we want update model for next frame, current frame we can't touch and update because it will be used on GPU
    animation->updateAnimation(1 - currentFrame, timer->getElapsedCurrent());
    loggerCPU->end();
  });

  loggerGPU->begin("Render terrain " + std::to_string(globalFrame), currentFrame);
  // for terrain we have to draw both: fill and normal/wireframe
  terrain->draw(currentFrame, commandBufferRender, settings->getDrawType());
  loggerGPU->end();

  loggerGPU->begin("Render spheres " + std::to_string(globalFrame), currentFrame);
  for (auto sphere : spheres) {
    sphere->draw(currentFrame, commandBufferRender);
  }
  loggerGPU->end();

  loggerGPU->begin("Render cube " + std::to_string(globalFrame), currentFrame);
  cube->draw(currentFrame, commandBufferRender);
  equiCube->draw(currentFrame, commandBufferRender);
  diffuseCube->draw(currentFrame, commandBufferRender);
  specularCube->draw(currentFrame, commandBufferRender);
  loggerGPU->end();

  // contains transparency, should be drawn last
  loggerGPU->begin("Render particles " + std::to_string(globalFrame), currentFrame);
  particleSystem->drawGraphic(currentFrame, commandBufferRender);
  loggerGPU->end();

  // should be drawn last
  loggerGPU->begin("Render skybox " + std::to_string(globalFrame), currentFrame);
  skybox->draw(currentFrame, commandBufferRender);
  loggerGPU->end();

  vkCmdEndRendering(commandBufferRender->getCommandBuffer()[currentFrame]);
  commandBufferRender->endCommands();
}

void initialize() {
  settings = std::make_shared<Settings>();
  settings->setName("Vulkan");
  settings->setResolution(std::tuple{1600, 900});
  // for HDR, linear 16 bit per channel to represent values outside of 0-1 range (UNORM - float [0, 1], SFLOAT - float)
  // https://registry.khronos.org/vulkan/specs/1.1/html/vkspec.html#_identification_of_formats
  settings->setGraphicColorFormat(VK_FORMAT_R32G32B32A32_SFLOAT);
  settings->setSwapchainColorFormat(VK_FORMAT_B8G8R8A8_UNORM);
  // SRGB the same as UNORM but + gamma conversion
  settings->setLoadTextureColorFormat(VK_FORMAT_R8G8B8A8_SRGB);
  settings->setLoadTextureAuxilaryFormat(VK_FORMAT_R8G8B8A8_UNORM);
  settings->setAnisotropicSamples(0);
  settings->setDepthFormat(VK_FORMAT_D32_SFLOAT);
  settings->setMaxFramesInFlight(2);
  settings->setThreadsInPool(6);
  settings->setDesiredFPS(1000);

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
  commandPoolRender = std::make_shared<CommandPool>(QueueType::GRAPHIC, device);
  commandBufferRender = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(), commandPoolRender, device);

  commandPoolEquirectangular = std::make_shared<CommandPool>(QueueType::GRAPHIC, device);
  commandBufferEquirectangular = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(),
                                                                 commandPoolEquirectangular, device);

  // TODO: transfer doesn't work
  commandPoolTransfer = std::make_shared<CommandPool>(QueueType::GRAPHIC, device);
  commandBufferTransfer = std::make_shared<CommandBuffer>(1, commandPoolTransfer, device);

  commandPoolParticleSystem = std::make_shared<CommandPool>(QueueType::COMPUTE, device);
  commandBufferParticleSystem = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(),
                                                                commandPoolParticleSystem, device);

  commandPoolPostprocessing = std::make_shared<CommandPool>(QueueType::COMPUTE, device);
  commandBufferPostprocessing = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(),
                                                                commandPoolPostprocessing, device);

  commandPoolGUI = std::make_shared<CommandPool>(QueueType::GRAPHIC, device);
  commandBufferGUI = std::make_shared<CommandBuffer>(settings->getMaxFramesInFlight(), commandPoolGUI, device);
  // start transfer command buffer
  commandBufferTransfer->beginCommands(0);

  //
  loggerGPU = std::make_shared<LoggerGPU>(state);
  loggerPostprocessing = std::make_shared<LoggerGPU>(state);
  loggerParticles = std::make_shared<LoggerGPU>(state);
  loggerGPUDebug = std::make_shared<LoggerGPU>(state);
  loggerCPU = std::make_shared<LoggerCPU>();

  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    // graphic-presentation
    imageAvailableSemaphores.push_back(std::make_shared<Semaphore>(VK_SEMAPHORE_TYPE_BINARY, device));
    renderFinishedSemaphores.push_back(std::make_shared<Semaphore>(VK_SEMAPHORE_TYPE_BINARY, device));
    inFlightFences.push_back(std::make_shared<Fence>(device));
    // compute-graphic
    particleSystemSemaphore.push_back(std::make_shared<Semaphore>(VK_SEMAPHORE_TYPE_TIMELINE, device));
    particleSystemFences.push_back(std::make_shared<Fence>(device));
    semaphorePostprocessing.push_back(std::make_shared<Semaphore>(VK_SEMAPHORE_TYPE_BINARY, device));
    semaphoreGUI.push_back(std::make_shared<Semaphore>(VK_SEMAPHORE_TYPE_BINARY, device));
  }

  graphicTexture.resize(settings->getMaxFramesInFlight());
  blurTextureIn.resize(settings->getMaxFramesInFlight());
  blurTextureOut.resize(settings->getMaxFramesInFlight());
  for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
    auto graphicImage = std::make_shared<Image>(settings->getResolution(), 1, 1, settings->getGraphicColorFormat(),
                                                VK_IMAGE_TILING_OPTIMAL,
                                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->getDevice());
    graphicImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                               commandBufferTransfer);
    auto graphicImageView = std::make_shared<ImageView>(graphicImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                        VK_IMAGE_ASPECT_COLOR_BIT, state->getDevice());
    graphicTexture[i] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, graphicImageView, state);
    {
      auto blurImage = std::make_shared<Image>(settings->getResolution(), 1, 1, settings->getGraphicColorFormat(),
                                               VK_IMAGE_TILING_OPTIMAL,
                                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->getDevice());
      blurImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                              commandBufferTransfer);
      auto blurImageView = std::make_shared<ImageView>(blurImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                       VK_IMAGE_ASPECT_COLOR_BIT, state->getDevice());
      blurTextureIn[i] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, blurImageView, state);
    }
    {
      auto blurImage = std::make_shared<Image>(settings->getResolution(), 1, 1, settings->getGraphicColorFormat(),
                                               VK_IMAGE_TILING_OPTIMAL,
                                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->getDevice());
      blurImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                              commandBufferTransfer);
      auto blurImageView = std::make_shared<ImageView>(blurImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                       VK_IMAGE_ASPECT_COLOR_BIT, state->getDevice());
      blurTextureOut[i] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, blurImageView, state);
    }
  }

  gui = std::make_shared<GUI>(window, state);
  gui->initialize(commandBufferTransfer);

  // for postprocessing descriptors GENERAL is needed
  swapchain->overrideImageLayout(VK_IMAGE_LAYOUT_GENERAL);
  postprocessing = std::make_shared<Postprocessing>(graphicTexture, blurTextureIn, swapchain->getImageViews(), state);
  // but we expect it to be in VK_IMAGE_LAYOUT_PRESENT_SRC_KHR as start value
  swapchain->changeImageLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, commandBufferTransfer);

  auto texture = std::make_shared<Texture>("../data/brickwall.jpg", settings->getLoadTextureColorFormat(),
                                           VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer, state);
  auto normalMap = std::make_shared<Texture>("../data/brickwall_normal.jpg", settings->getLoadTextureAuxilaryFormat(),
                                             VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer, state);
  cameraOrtho = std::make_shared<CameraOrtho>();
  cameraOrtho->setProjectionParameters({-1, 1, -1, 1}, 0, 1);
  cameraOrtho->setViewParameters(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));

  camera = std::make_shared<CameraFly>(settings);
  camera->setProjectionParameters(60.f, 0.1f, 100.f);
  input->subscribe(std::dynamic_pointer_cast<InputSubscriber>(camera));
  input->subscribe(std::dynamic_pointer_cast<InputSubscriber>(gui));
  lightManager = std::make_shared<LightManager>(commandBufferTransfer, state);
  pointLightHorizontal = lightManager->createPointLight(settings->getDepthResolution());
  pointLightHorizontal->setColor(glm::vec3(1.f, 1.f, 1.f));
  pointLightHorizontal->setPosition({3.f, 4.f, 0.f});
  /*pointLightVertical = lightManager->createPointLight(settings->getDepthResolution());
  pointLightVertical->createPhong(glm::vec3(0.f), glm::vec3(1.f), glm::vec3(1.f), glm::vec3(1.f, 1.f, 1.f));
  pointLightVertical->setPosition({-3.f, 4.f, 0.f});

  pointLightHorizontal2 = lightManager->createPointLight(settings->getDepthResolution());
  pointLightHorizontal2->createPhong(glm::vec3(0.f), glm::vec3(1.f), glm::vec3(1.f), glm::vec3(1.f, 1.f, 1.f));
  pointLightHorizontal2->setPosition({3.f, 4.f, 3.f});

  pointLightVertical2 = lightManager->createPointLight(settings->getDepthResolution());
  pointLightVertical2->createPhong(glm::vec3(0.f), glm::vec3(1.f), glm::vec3(1.f), glm::vec3(1.f, 1.f, 1.f));
  pointLightVertical2->setPosition({-3.f, 4.f, -3.f});*/

  ambientLight = lightManager->createAmbientLight();
  // calculate ambient color with default gamma
  float ambientColor = std::pow(0.05f, postprocessing->getGamma());
  ambientLight->setColor(glm::vec3(ambientColor, ambientColor, ambientColor));

  directionalLight = lightManager->createDirectionalLight(settings->getDepthResolution());
  directionalLight->setColor(glm::vec3(1.f, 1.f, 1.f));
  directionalLight->setPosition({0.f, 35.f, 0.f});
  directionalLight->setCenter({0.f, 0.f, 0.f});
  directionalLight->setUp({0.f, 0.f, -1.f});

  /*directionalLight2 = lightManager->createDirectionalLight(settings->getDepthResolution());
  directionalLight2->createPhong(glm::vec3(0.f), glm::vec3(1.f), glm::vec3(1.f), glm::vec3(1.f, 1.f, 1.f));
  directionalLight2->setPosition({3.f, 15.f, 0.f});
  directionalLight2->setCenter({0.f, 0.f, 0.f});
  directionalLight2->setUp({0.f, 1.f, 0.f});*/

  spriteManager = std::make_shared<SpriteManager>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, lightManager,
      commandBufferTransfer, state);
  spriteManagerHUD = std::make_shared<SpriteManager>(std::vector{settings->getGraphicColorFormat()}, lightManager,
                                                     commandBufferTransfer, state);
  modelManager = std::make_shared<Model3DManager>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, lightManager,
      commandBufferTransfer, state);
  debugVisualization = std::make_shared<DebugVisualization>(camera, gui, commandBufferTransfer, state);
  debugVisualization->setLights(lightManager);
  input->subscribe(std::dynamic_pointer_cast<InputSubscriber>(debugVisualization));

  auto shader = std::make_shared<Shader>(device);
  shader->add("../shaders/specularBRDF_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader->add("../shaders/specularBRDF_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  auto cameraSetLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
  cameraSetLayout->createUniformBuffer();

  auto spriteHUD = spriteManagerHUD->createSprite(shader, {{"camera", cameraSetLayout}});
  spriteHUD->setModel(glm::scale(glm::mat4(1.f), glm::vec3(2.f, 2.f, 1.f)));
  spriteManagerHUD->registerSprite(spriteHUD);

  {
    auto material = std::make_shared<MaterialPhong>(commandBufferTransfer, state);
    material->setBaseColor(texture);
    material->setNormal(normalMap);

    auto spriteForward = spriteManager->createSprite();
    spriteForward->setMaterial(material);
    auto spriteBackward = spriteManager->createSprite();
    spriteBackward->setMaterial(material);

    auto spriteLeft = spriteManager->createSprite();
    spriteLeft->setMaterial(material);
    auto spriteRight = spriteManager->createSprite();
    spriteRight->setMaterial(material);

    auto spriteTop = spriteManager->createSprite();
    spriteTop->setMaterial(material);
    auto spriteBot = spriteManager->createSprite();
    spriteBot->setMaterial(material);

    {
      glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, 1.f));
      spriteForward->setModel(model);
    }
    {
      auto model = glm::rotate(glm::mat4(1.f), glm::radians(180.f), glm::vec3(0.f, 1.f, 0.f));
      spriteBackward->setModel(model);
    }
    {
      glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(-0.5f, 0.f, 0.5f));
      model = glm::rotate(model, glm::radians(-90.f), glm::vec3(0.f, 1.f, 0.f));
      spriteLeft->setModel(model);
    }
    {
      glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(0.5f, 0.f, 0.5f));
      model = glm::rotate(model, glm::radians(90.f), glm::vec3(0.f, 1.f, 0.f));
      spriteRight->setModel(model);
    }
    {
      glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(0.0f, 0.5f, 0.5f));
      model = glm::rotate(model, glm::radians(-90.f), glm::vec3(1.f, 0.f, 0.f));
      spriteTop->setModel(model);
    }
    {
      glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(0.0f, -0.5f, 0.5f));
      model = glm::rotate(model, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
      spriteBot->setModel(model);
    }

    spriteManager->registerSprite(spriteForward);
    spriteManager->registerSprite(spriteBackward);
    spriteManager->registerSprite(spriteLeft);
    spriteManager->registerSprite(spriteRight);
    spriteManager->registerSprite(spriteTop);
    spriteManager->registerSprite(spriteBot);
  }
  std::shared_ptr<Loader> loaderGLTF = std::make_shared<Loader>("../data/DamagedHelmet/DamagedHelmet.gltf",
                                                                commandBufferTransfer, state);
  std::shared_ptr<Loader> loaderGLTFBox = std::make_shared<Loader>("../data/Box/Box.gltf", commandBufferTransfer,
                                                                   state);
  /*for (auto& mesh : loaderGLTF->getMeshes())
    mesh->setColor(glm::vec3(1.f, 0.f, 0.f));*/
  auto modelGLTFPhong = modelManager->createModel3D(loaderGLTF->getNodes(), loaderGLTF->getMeshes());
  modelManager->registerModel3D(modelGLTFPhong);
  modelGLTFPhong->setMaterial(loaderGLTF->getMaterialsPhong());

  auto modelGLTFPBR = modelManager->createModel3D(loaderGLTF->getNodes(), loaderGLTF->getMeshes());
  modelManager->registerModel3D(modelGLTFPBR);
  auto pbrMaterial = loaderGLTF->getMaterialsPBR();
  modelGLTFPBR->setMaterial(pbrMaterial);

  auto modelBox = modelManager->createModel3D(loaderGLTFBox->getNodes(), loaderGLTFBox->getMeshes());
  // modelManager->registerModel3D(modelBox);
  modelBox->setMaterial(loaderGLTFBox->getMaterialsPBR());

  animation = std::make_shared<Animation>(loaderGLTF->getNodes(), loaderGLTF->getSkins(), loaderGLTF->getAnimations(),
                                          state);
  // modelGLTFPhong->setAnimation(animation);
  modelGLTFPBR->setAnimation(animation);

  // modelGLTF = modelManager->createModel3D("../data/Avocado/Avocado.gltf");
  // modelGLTF = modelManager->createModel3D("../data/CesiumMan/CesiumMan.gltf");
  // modelGLTF = modelManager->createModel3D("../data/BrainStem/BrainStem.gltf");
  // modelGLTF = modelManager->createModel3D("../data/SimpleSkin/SimpleSkin.gltf");
  // modelGLTF = modelManager->createModel3D("../data/Sponza/Sponza.gltf");
  // modelGLTF = modelManager->createModel3D("../data/DamagedHelmet/DamagedHelmet.gltf");
  // modelGLTF = modelManager->createModel3D("../data/Box/BoxTextured.gltf");
  //{
  //   glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(-2.f, -1.f, 0.f));
  //   // model = glm::rotate(model, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
  //   model3D->setModel(model);
  // }
  {
    glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(-2.f, 2.f, -5.f));
    // model = glm::rotate(model, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
    model = glm::scale(model, glm::vec3(1.f, 1.f, 1.f));
    modelGLTFPhong->setModel(model);
  }
  {
    glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(-2.f, 2.f, -3.f));
    // model = glm::rotate(model, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
    model = glm::scale(model, glm::vec3(1.f, 1.f, 1.f));
    modelGLTFPBR->setModel(model);
  }
  {
    glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 2.f, -3.f));
    modelBox->setModel(model);
  }

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

  terrain = std::make_shared<TerrainGPU>(
      std::pair{12, 12}, std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
      commandBufferTransfer, lightManager, state);
  {
    auto scaleMatrix = glm::scale(glm::mat4(1.f), glm::vec3(0.1f, 0.1f, 0.1f));
    auto translateMatrix = glm::translate(scaleMatrix, glm::vec3(2.f, -6.f, 0.f));
    terrain->setModel(translateMatrix);
  }
  terrain->setCamera(camera);

  auto particleTexture = std::make_shared<Texture>("../data/Particles/gradient.png",
                                                   settings->getLoadTextureAuxilaryFormat(),
                                                   VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer, state);
  particleSystem = std::make_shared<ParticleSystem>(
      300, std::vector{state->getSettings()->getGraphicColorFormat(), state->getSettings()->getGraphicColorFormat()},
      particleTexture, commandBufferTransfer, state);
  {
    auto matrix = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, 2.f));
    matrix = glm::scale(matrix, glm::vec3(0.5f, 0.5f, 0.5f));

    particleSystem->setModel(matrix);
  }
  particleSystem->setCamera(camera);

  spheres.resize(6);
  // non HDR
  spheres[0] = std::make_shared<Sphere>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, VK_CULL_MODE_BACK_BIT,
      VK_POLYGON_MODE_FILL, commandBufferTransfer, state);
  spheres[0]->getMesh()->setColor({{0.f, 0.f, 0.1f}}, commandBufferTransfer);
  spheres[0]->setCamera(camera);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -5.f, 0.f));
    spheres[0]->setModel(model);
  }
  spheres[1] = std::make_shared<Sphere>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, VK_CULL_MODE_BACK_BIT,
      VK_POLYGON_MODE_FILL, commandBufferTransfer, state);
  spheres[1]->getMesh()->setColor({{0.f, 0.f, 0.5f}}, commandBufferTransfer);
  spheres[1]->setCamera(camera);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(2.f, -5.f, 0.f));
    spheres[1]->setModel(model);
  }
  spheres[2] = std::make_shared<Sphere>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, VK_CULL_MODE_BACK_BIT,
      VK_POLYGON_MODE_FILL, commandBufferTransfer, state);
  spheres[2]->getMesh()->setColor({{0.f, 0.f, 10.f}}, commandBufferTransfer);
  spheres[2]->setCamera(camera);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(-2.f, -5.f, 0.f));
    spheres[2]->setModel(model);
  }
  // HDR
  spheres[3] = std::make_shared<Sphere>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, VK_CULL_MODE_BACK_BIT,
      VK_POLYGON_MODE_FILL, commandBufferTransfer, state);
  spheres[3]->getMesh()->setColor({{5.f, 0.f, 0.f}}, commandBufferTransfer);
  spheres[3]->setCamera(camera);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -5.f, 2.f));
    spheres[3]->setModel(model);
  }
  spheres[4] = std::make_shared<Sphere>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, VK_CULL_MODE_BACK_BIT,
      VK_POLYGON_MODE_FILL, commandBufferTransfer, state);
  spheres[4]->getMesh()->setColor({{0.f, 5.f, 0.f}}, commandBufferTransfer);
  spheres[4]->setCamera(camera);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -5.f, -2.f));
    spheres[4]->setModel(model);
  }
  spheres[5] = std::make_shared<Sphere>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, VK_CULL_MODE_BACK_BIT,
      VK_POLYGON_MODE_FILL, commandBufferTransfer, state);
  spheres[5]->getMesh()->setColor({{0.f, 0.f, 20.f}}, commandBufferTransfer);
  spheres[5]->setCamera(camera);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(-4.f, -5.f, 0.f));
    spheres[5]->setModel(model);
  }

  auto [width, height] = settings->getResolution();

  cubemapEquirectangular = std::make_shared<Cubemap>(
      settings->getDepthResolution(), settings->getGraphicColorFormat(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      commandBufferTransfer, state);
  cubemapDiffuse = std::make_shared<Cubemap>(settings->getDiffuseIBLResolution(), settings->getGraphicColorFormat(), 1,
                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
                                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                             commandBufferTransfer, state);

  cubemapSpecular = std::make_shared<Cubemap>(
      settings->getSpecularIBLResolution(), settings->getGraphicColorFormat(), settings->getSpecularMipMap(),
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);

  auto brdfImage = std::make_shared<Image>(settings->getDepthResolution(), 1, 1, settings->getGraphicColorFormat(),
                                           VK_IMAGE_TILING_OPTIMAL,
                                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->getDevice());
  brdfImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                          commandBufferTransfer);
  auto brdfImageView = std::make_shared<ImageView>(brdfImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1,
                                                   VK_IMAGE_ASPECT_COLOR_BIT, state->getDevice());
  auto brdfTexture = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, brdfImageView, state);

  cubemap = std::make_shared<Cubemap>(
      std::vector<std::string>{"../data/Skybox/right.jpg", "../data/Skybox/left.jpg", "../data/Skybox/top.jpg",
                               "../data/Skybox/bottom.jpg", "../data/Skybox/front.jpg", "../data/Skybox/back.jpg"},
      settings->getLoadTextureColorFormat(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);
  cube = std::make_shared<Cube>(std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
                                VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, lightManager, commandBufferTransfer, state);
  skybox = std::make_shared<Skybox>(std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
                                    VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, commandBufferTransfer, state);
  auto materialColor = std::make_shared<MaterialColor>(commandBufferTransfer, state);
  materialColor->setBaseColor(cubemap->getTexture());
  cube->setMaterial(materialColor);
  cube->setCamera(camera);
  skybox->setCamera(camera);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 3.f, 0.f));
    cube->setModel(model);
  }

  cubeTemp = std::make_shared<Cube>(std::vector{settings->getGraphicColorFormat()}, VK_CULL_MODE_NONE,
                                    VK_POLYGON_MODE_FILL, lightManager, commandBufferTransfer, state);
  auto cameraTemp = std::make_shared<CameraFly>(settings);
  cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, 1.f, 0.f));
  cameraTemp->setProjectionParameters(90.f, 0.1f, 100.f);
  cameraTemp->setAspect(1.f);
  cubeTemp->setCamera(cameraTemp);
  {
    auto model = glm::translate(glm::mat4(1.f), cameraTemp->getEye());
    cubeTemp->setModel(model);
  }

  equiCube = std::make_shared<Cube>(std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()},
                                    VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, lightManager, commandBufferTransfer,
                                    state);
  equiCube->setCamera(camera);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 3.f, -3.f));
    equiCube->setModel(model);
  }

  diffuseCube = std::make_shared<Cube>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, VK_CULL_MODE_NONE,
      VK_POLYGON_MODE_FILL, lightManager, commandBufferTransfer, state);
  diffuseCube->setCamera(camera);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(2.f, 3.f, -3.f));
    diffuseCube->setModel(model);
  }

  specularCube = std::make_shared<Cube>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, VK_CULL_MODE_NONE,
      VK_POLYGON_MODE_FILL, lightManager, commandBufferTransfer, state);
  specularCube->setCamera(camera);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(4.f, 3.f, -3.f));
    specularCube->setModel(model);
  }

  equirectangular = std::make_shared<Equirectangular>("../data/Skybox/newport_loft.hdr", commandBufferTransfer, state);
  auto materialEq = std::make_shared<MaterialPhong>(commandBufferTransfer, state);
  materialEq->setBaseColor(equirectangular->getTexture());
  auto materialColorEq = std::make_shared<MaterialColor>(commandBufferTransfer, state);
  materialColorEq->setBaseColor(equirectangular->getTexture());
  cubeTemp->setMaterial(materialColorEq);

  blur = std::make_shared<Blur>(blurTextureIn, blurTextureOut, state);
  debugVisualization->setPostprocessing(postprocessing);
  pool = std::make_shared<BS::thread_pool>(settings->getThreadsInPool());

  commandBufferTransfer->endCommands();
  commandBufferTransfer->submitToQueue(true);

  {
    // render equirectangular to cubemap
    commandBufferEquirectangular->beginCommands(currentFrame);
    loggerGPU->setCommandBufferName("Draw equirectangular buffer", currentFrame, commandBufferEquirectangular);
    /////////////////////////////////////////////////////////////////////////////////////////
    // render graphic
    /////////////////////////////////////////////////////////////////////////////////////////
    for (int i = 0; i < 6; i++) {
      auto currentTexture = cubemapEquirectangular->getTextureSeparate()[i][0];
      VkClearValue clearColor;
      clearColor.color = settings->getClearColor();
      std::vector<VkRenderingAttachmentInfo> colorAttachmentInfo(1);
      // here we render scene as is
      colorAttachmentInfo[0] = VkRenderingAttachmentInfo{
          .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
          .imageView = currentTexture->getImageView()->getImageView(),
          .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
          .clearValue = clearColor,
      };

      auto [width, height] = currentTexture->getImageView()->getImage()->getResolution();
      VkRect2D renderArea{};
      renderArea.extent.width = width;
      renderArea.extent.height = height;
      const VkRenderingInfo renderInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                       .renderArea = renderArea,
                                       .layerCount = 1,
                                       .colorAttachmentCount = (uint32_t)colorAttachmentInfo.size(),
                                       .pColorAttachments = colorAttachmentInfo.data()};

      vkCmdBeginRendering(commandBufferEquirectangular->getCommandBuffer()[currentFrame], &renderInfo);

      loggerGPU->begin("Render equirectangular", currentFrame);
      // up is inverted for X and Z because of some specific cubemap Y coordinate stuff
      switch (i) {
        case 0:
          // POSITIVE_X / right
          cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(1.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f));
          break;
        case 1:
          // NEGATIVE_X /left
          cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(-1.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f));
          break;
        case 2:
          // POSITIVE_Y / top
          cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
          break;
        case 3:
          // NEGATIVE_Y / bottom
          cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f), glm::vec3(0.f, 0.f, -1.f));
          break;
        case 4:
          // POSITIVE_Z / near
          cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f), glm::vec3(0.f, -1.f, 0.f));
          break;
        case 5:
          // NEGATIVE_Z / far
          cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, -1.f, 0.f));
          break;
      }

      cubeTemp->drawEquirectangular(currentFrame, commandBufferEquirectangular, i);
      loggerGPU->end();

      vkCmdEndRendering(commandBufferEquirectangular->getCommandBuffer()[currentFrame]);
    }
    commandBufferEquirectangular->endCommands();
    commandBufferEquirectangular->submitToQueue(true);
  }

  commandBufferTransfer->beginCommands(0);
  auto materialColorCM = std::make_shared<MaterialColor>(commandBufferTransfer, state);
  auto materialColorDiffuse = std::make_shared<MaterialColor>(commandBufferTransfer, state);
  auto materialColorSpecular = std::make_shared<MaterialColor>(commandBufferTransfer, state);
  materialColorCM->setBaseColor(cubemapEquirectangular->getTexture());
  cubeTemp->setMaterial(materialColorCM);
  equiCube->setMaterial(materialColorCM);
  skybox->setMaterial(materialColorCM);
  commandBufferTransfer->endCommands();
  commandBufferTransfer->submitToQueue(true);

  {
    // render cubemap to diffuse
    commandBufferEquirectangular->beginCommands(currentFrame);
    loggerGPU->setCommandBufferName("Draw diffuse buffer", currentFrame, commandBufferEquirectangular);
    /////////////////////////////////////////////////////////////////////////////////////////
    // render graphic
    /////////////////////////////////////////////////////////////////////////////////////////
    for (int i = 0; i < 6; i++) {
      auto currentTexture = cubemapDiffuse->getTextureSeparate()[i][0];
      VkClearValue clearColor;
      clearColor.color = settings->getClearColor();
      std::vector<VkRenderingAttachmentInfo> colorAttachmentInfo(1);
      // here we render scene as is
      colorAttachmentInfo[0] = VkRenderingAttachmentInfo{
          .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
          .imageView = currentTexture->getImageView()->getImageView(),
          .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
          .clearValue = clearColor,
      };

      auto [width, height] = currentTexture->getImageView()->getImage()->getResolution();
      VkRect2D renderArea{};
      renderArea.extent.width = width;
      renderArea.extent.height = height;
      const VkRenderingInfo renderInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                       .renderArea = renderArea,
                                       .layerCount = 1,
                                       .colorAttachmentCount = (uint32_t)colorAttachmentInfo.size(),
                                       .pColorAttachments = colorAttachmentInfo.data()};

      vkCmdBeginRendering(commandBufferEquirectangular->getCommandBuffer()[currentFrame], &renderInfo);

      loggerGPU->begin("Render diffuse", currentFrame);
      // up is inverted for X and Z because of some specific cubemap Y coordinate stuff
      switch (i) {
        case 0:
          // POSITIVE_X / right
          cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(1.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f));
          break;
        case 1:
          // NEGATIVE_X /left
          cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(-1.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f));
          break;
        case 2:
          // POSITIVE_Y / top
          cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
          break;
        case 3:
          // NEGATIVE_Y / bottom
          cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f), glm::vec3(0.f, 0.f, -1.f));
          break;
        case 4:
          // POSITIVE_Z / near
          cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f), glm::vec3(0.f, -1.f, 0.f));
          break;
        case 5:
          // NEGATIVE_Z / far
          cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, -1.f, 0.f));
          break;
      }

      cubeTemp->drawDiffuse(currentFrame, commandBufferEquirectangular, i);
      loggerGPU->end();

      vkCmdEndRendering(commandBufferEquirectangular->getCommandBuffer()[currentFrame]);
    }
    commandBufferEquirectangular->endCommands();
    commandBufferEquirectangular->submitToQueue(true);
  }

  // render to specular
  {
    commandBufferEquirectangular->beginCommands(currentFrame);
    loggerGPU->setCommandBufferName("Draw specular buffer", currentFrame, commandBufferEquirectangular);
    /////////////////////////////////////////////////////////////////////////////////////////
    // render graphic
    /////////////////////////////////////////////////////////////////////////////////////////
    for (int i = 0; i < 6; i++) {
      for (int j = 0; j < settings->getSpecularMipMap(); j++) {
        auto currentTexture = cubemapSpecular->getTextureSeparate()[i][j];
        VkClearValue clearColor;
        clearColor.color = settings->getClearColor();
        std::vector<VkRenderingAttachmentInfo> colorAttachmentInfo(1);
        // here we render scene as is
        colorAttachmentInfo[0] = VkRenderingAttachmentInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = currentTexture->getImageView()->getImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = clearColor,
        };

        auto [width, height] = currentTexture->getImageView()->getImage()->getResolution();
        VkRect2D renderArea{};
        renderArea.extent.width = width * std::pow(0.5, j);
        renderArea.extent.height = height * std::pow(0.5, j);
        const VkRenderingInfo renderInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                         .renderArea = renderArea,
                                         .layerCount = 1,
                                         .colorAttachmentCount = (uint32_t)colorAttachmentInfo.size(),
                                         .pColorAttachments = colorAttachmentInfo.data()};

        vkCmdBeginRendering(commandBufferEquirectangular->getCommandBuffer()[currentFrame], &renderInfo);

        loggerGPU->begin("Render specular", currentFrame);
        // up is inverted for X and Z because of some specific cubemap Y coordinate stuff
        switch (i) {
          case 0:
            // POSITIVE_X / right
            cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(1.f, 0.f, 0.f),
                                          glm::vec3(0.f, -1.f, 0.f));
            break;
          case 1:
            // NEGATIVE_X /left
            cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(-1.f, 0.f, 0.f),
                                          glm::vec3(0.f, -1.f, 0.f));
            break;
          case 2:
            // POSITIVE_Y / top
            cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
            break;
          case 3:
            // NEGATIVE_Y / bottom
            cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f),
                                          glm::vec3(0.f, 0.f, -1.f));
            break;
          case 4:
            // POSITIVE_Z / near
            cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f),
                                          glm::vec3(0.f, -1.f, 0.f));
            break;
          case 5:
            // NEGATIVE_Z / far
            cameraTemp->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, -1.f),
                                          glm::vec3(0.f, -1.f, 0.f));
            break;
        }

        cubeTemp->drawSpecular(currentFrame, commandBufferEquirectangular, i, j);
        loggerGPU->end();

        vkCmdEndRendering(commandBufferEquirectangular->getCommandBuffer()[currentFrame]);
      }
    }
    commandBufferEquirectangular->endCommands();
    commandBufferEquirectangular->submitToQueue(true);
  }

  // display specular as texture
  materialColorSpecular->setBaseColor(cubemapSpecular->getTexture());
  specularCube->setMaterial(materialColorSpecular);

  // display diffuse as texture
  materialColorDiffuse->setBaseColor(cubemapDiffuse->getTexture());
  diffuseCube->setMaterial(materialColorDiffuse);

  // set diffuse to material
  for (auto& material : pbrMaterial) {
    material->setDiffuseIBL(cubemapDiffuse->getTexture());
  }

  // render to specular
  {
    commandBufferEquirectangular->beginCommands(currentFrame);
    loggerGPU->setCommandBufferName("Draw specular brdf", currentFrame, commandBufferEquirectangular);
    /////////////////////////////////////////////////////////////////////////////////////////
    // render graphic
    /////////////////////////////////////////////////////////////////////////////////////////

    VkClearValue clearColor;
    clearColor.color = settings->getClearColor();
    std::vector<VkRenderingAttachmentInfo> colorAttachmentInfo(1);
    // here we render scene as is
    colorAttachmentInfo[0] = VkRenderingAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = brdfTexture->getImageView()->getImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clearColor,
    };

    auto [width, height] = brdfTexture->getImageView()->getImage()->getResolution();
    VkRect2D renderArea{};
    renderArea.extent.width = width;
    renderArea.extent.height = height;
    const VkRenderingInfo renderInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                     .renderArea = renderArea,
                                     .layerCount = 1,
                                     .colorAttachmentCount = (uint32_t)colorAttachmentInfo.size(),
                                     .pColorAttachments = colorAttachmentInfo.data()};

    vkCmdBeginRendering(commandBufferEquirectangular->getCommandBuffer()[currentFrame], &renderInfo);

    loggerGPU->begin("Render specular brdf", currentFrame);
    spriteManagerHUD->setCamera(cameraOrtho);
    spriteManagerHUD->draw(currentFrame, brdfTexture->getImageView()->getImage()->getResolution(),
                           commandBufferEquirectangular, DrawType::FILL);
    loggerGPU->end();

    vkCmdEndRendering(commandBufferEquirectangular->getCommandBuffer()[currentFrame]);

    commandBufferEquirectangular->endCommands();
    commandBufferEquirectangular->submitToQueue(true);
  }

  // set diffuse to material
  /*for (auto& material : pbrMaterial) {
    material->setSpecularIBL(cubemapSpecular->getTexture(), brdfTexture);
  }*/

  commandBufferTransfer->beginCommands(0);
  auto materialBRDF = std::make_shared<MaterialPhong>(commandBufferTransfer, state);
  materialBRDF->setBaseColor(brdfTexture);
  materialBRDF->setCoefficients(glm::vec3(1.f), glm::vec3(0.f), glm::vec3(0.f), 0.f);
  auto spriteBRDF = spriteManager->createSprite();
  spriteBRDF->enableLighting(false);
  spriteBRDF->enableShadow(false);
  spriteBRDF->enableDepth(false);
  spriteBRDF->setMaterial(materialBRDF);
  {
    glm::mat4 model = glm::translate(glm::mat4(1.f), glm::vec3(5.f, 3.f, 1.f));
    spriteBRDF->setModel(model);
  }
  spriteManager->registerSprite(spriteBRDF);
  commandBufferTransfer->endCommands();
  commandBufferTransfer->submitToQueue(true);
}

void getImageIndex(uint32_t* imageIndex) {
  currentFrame = timer->getFrameCounter() % settings->getMaxFramesInFlight();
  std::vector<VkFence> waitFences = {inFlightFences[currentFrame]->getFence()};
  auto result = vkWaitForFences(device->getLogicalDevice(), waitFences.size(), waitFences.data(), VK_TRUE, UINT64_MAX);
  if (result != VK_SUCCESS) throw std::runtime_error("Can't wait for fence");

  result = vkResetFences(device->getLogicalDevice(), waitFences.size(), waitFences.data());
  if (result != VK_SUCCESS) throw std::runtime_error("Can't reset fence");
  // RETURNS ONLY INDEX, NOT IMAGE
  // semaphore to signal, once image is available
  result = vkAcquireNextImageKHR(device->getLogicalDevice(), swapchain->getSwapchain(), UINT64_MAX,
                                 imageAvailableSemaphores[currentFrame]->getSemaphore(), VK_NULL_HANDLE, imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    // TODO: recreate swapchain
    throw std::runtime_error("failed to acquire swap chain image!");
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }
}

void displayFrame(uint32_t* imageIndex) {
  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  std::vector<VkSemaphore> waitSemaphoresPresent = {renderFinishedSemaphores[currentFrame]->getSemaphore()};

  presentInfo.waitSemaphoreCount = waitSemaphoresPresent.size();
  presentInfo.pWaitSemaphores = waitSemaphoresPresent.data();

  VkSwapchainKHR swapChains[] = {swapchain->getSwapchain()};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;

  presentInfo.pImageIndices = imageIndex;

  // TODO: change to own present queue
  auto result = vkQueuePresentKHR(device->getQueue(QueueType::PRESENT), &presentInfo);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    // TODO: support window resize
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }
}

void drawFrame(int imageIndex) {
  currentFrame = timer->getFrameCounter() % settings->getMaxFramesInFlight();

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

  // submit compute particles
  auto particlesFuture = pool->submit(computeParticles);

  /////////////////////////////////////////////////////////////////////////////////////////
  // render to depth buffer
  /////////////////////////////////////////////////////////////////////////////////////////
  std::vector<std::future<void>> shadowFutures;
  for (int i = 0; i < lightManager->getDirectionalLights().size(); i++) {
    shadowFutures.push_back(pool->submit(directionalLightCalculator, i));
  }

  for (int i = 0; i < lightManager->getPointLights().size(); i++) {
    for (int j = 0; j < 6; j++) {
      shadowFutures.push_back(pool->submit(pointLightCalculator, i, j));
    }
  }

  auto postprocessingFuture = pool->submit([imageIndex]() { computePostprocessing(imageIndex); });
  auto debugVisualizationFuture = pool->submit(debugVisualizations, imageIndex);

  /////////////////////////////////////////////////////////////////////////////////////////////////
  // Render graphics
  /////////////////////////////////////////////////////////////////////////////////////////////////
  renderGraphic();

  // wait for shadow to complete before render
  for (auto& shadowFuture : shadowFutures) {
    if (shadowFuture.valid()) {
      shadowFuture.get();
    }
  }

  // wait for particles to complete before render
  if (particlesFuture.valid()) particlesFuture.get();

  VkSemaphore signalSemaphores[] = {semaphorePostprocessing[currentFrame]->getSemaphore()};
  std::vector<VkSemaphore> waitSemaphores = {particleSystemSemaphore[currentFrame]->getSemaphore()};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT};
  std::vector<uint64_t> waitSemaphoreValues = {particleSignal};

  VkTimelineSemaphoreSubmitInfo waitParticlesSemaphore{};
  waitParticlesSemaphore.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
  waitParticlesSemaphore.pNext = NULL;
  waitParticlesSemaphore.waitSemaphoreValueCount = waitSemaphores.size();
  waitParticlesSemaphore.pWaitSemaphoreValues = waitSemaphoreValues.data();
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.pNext = &waitParticlesSemaphore;
  submitInfo.waitSemaphoreCount = waitSemaphores.size();
  submitInfo.pWaitSemaphores = waitSemaphores.data();
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBufferRender->getCommandBuffer()[currentFrame];
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  commandBufferRender->submitToQueue(submitInfo, nullptr);

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Render compute postprocessing
  //////////////////////////////////////////////////////////////////////////////////////////////////
  {
    if (postprocessingFuture.valid()) postprocessingFuture.get();

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
    commandBufferPostprocessing->submitToQueue(submitInfoPostprocessing, nullptr);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // Render debug visualization
  //////////////////////////////////////////////////////////////////////////////////////////////////
  {
    if (debugVisualizationFuture.valid()) debugVisualizationFuture.get();

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
    commandBufferGUI->submitToQueue(submitInfo, inFlightFences[currentFrame]);
  }
}

void mainLoop() {
  while (!glfwWindowShouldClose(window->getWindow())) {
    glfwPollEvents();
    timer->tick();
    timerFPSReal->tick();
    timerFPSLimited->tick();

    uint32_t imageIndex;
    getImageIndex(&imageIndex);
    drawFrame(imageIndex);
    timerFPSReal->tock();
    // if GPU frames are limited by driver it will happen during display
    displayFrame(&imageIndex);

    if (FPSChanged) {
      FPSChanged = false;
      timer->reset();
    }

    timer->sleep(settings->getDesiredFPS(), loggerCPU);
    timer->tock();
    timerFPSLimited->tock();
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