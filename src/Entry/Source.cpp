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

#undef near
#undef far

float fps = 0;
float frameTimer = 0.f;
uint64_t currentFrame = 0;
uint64_t globalFrame = 0;
uint64_t sleepFrame = 0;

// Depth bias (and slope) are used to avoid shadowing artifacts
// Constant depth bias factor (always applied)
float depthBiasConstant = 1.25f;
// Slope depth bias factor, applied depending on polygon's slope
float depthBiasSlope = 1.75f;

std::shared_ptr<Window> window;
std::shared_ptr<Instance> instance;
std::shared_ptr<Device> device;
std::shared_ptr<CommandBuffer> commandBuffer, commandBufferTransfer, commandBufferParticleSystem,
    commandBufferPostprocessing, commandBufferBlur, commandBufferGUI;
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

std::shared_ptr<SpriteManager> spriteManager;
std::shared_ptr<Model3DManager> modelManager;
std::shared_ptr<ModelGLTF> modelGLTF;
std::shared_ptr<ParticleSystem> particleSystem;
std::shared_ptr<Postprocessing> postprocessing;
std::shared_ptr<Blur> blur;
std::shared_ptr<Input> input;
std::shared_ptr<GUI> gui;
std::shared_ptr<Swapchain> swapchain;
std::shared_ptr<CameraFly> camera;
std::shared_ptr<LightManager> lightManager;
std::shared_ptr<PointLight> pointLightHorizontal, pointLightVertical, pointLightHorizontal2, pointLightVertical2;
std::shared_ptr<DirectionalLight> directionalLight, directionalLight2;
std::shared_ptr<DebugVisualization> debugVisualization;
std::shared_ptr<LoggerGPU> loggerGPU, loggerPostprocessing, loggerParticles, loggerGPUDebug;
std::shared_ptr<LoggerCPU> loggerCPU;

std::future<void> updateJoints;

std::vector<std::shared_ptr<Texture>> graphicTexture, blurTextureIn, blurTextureOut;

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
std::mutex debugVisualizationMutex;
uint64_t particleSignal;
int desiredFPS = 1000;
bool FPSChanged = false;

std::vector<std::shared_ptr<Sphere>> spheres;

void directionalLightCalculator(int index) {
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
  loggerGPU->end();
  loggerGPU->begin("Models to directional depth buffer " + std::to_string(globalFrame), currentFrame);
  modelManager->drawShadow(currentFrame, commandBuffer, LightType::DIRECTIONAL, index);
  loggerGPU->end();
  loggerGPU->begin("Terrain to directional depth buffer " + std::to_string(globalFrame), currentFrame);
  terrain->drawShadow(currentFrame, commandBuffer, LightType::DIRECTIONAL, index);
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
  loggerGPU->end();
  loggerGPU->begin("Models to point depth buffer " + std::to_string(globalFrame), currentFrame);
  modelManager->drawShadow(currentFrame, commandBuffer, LightType::POINT, index, face);
  loggerGPU->end();
  loggerGPU->begin("Terrain to point depth buffer " + std::to_string(globalFrame), currentFrame);
  terrain->drawShadow(currentFrame, commandBuffer, LightType::POINT, index, face);
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

  loggerParticles->begin("Particle system compute " + std::to_string(globalFrame), currentFrame);
  particleSystem->drawCompute(currentFrame, commandBufferParticleSystem);
  loggerParticles->end();

  particleSystem->updateTimer(frameTimer);

  particleSignal = globalFrame + 1;
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
    loggerPostprocessing->begin("Blur horizontal compute " + std::to_string(globalFrame), currentFrame);
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

    loggerPostprocessing->begin("Blur vertical compute " + std::to_string(globalFrame), currentFrame);
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

  loggerPostprocessing->begin("Postprocessing compute " + std::to_string(globalFrame), currentFrame);
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

  vkCmdBeginRendering(commandBufferGUI->getCommandBuffer()[currentFrame], &renderInfo);
  loggerGPUDebug->begin("Render debug visualization " + std::to_string(globalFrame), currentFrame);
  debugVisualization->draw(currentFrame, commandBufferGUI);
  loggerGPUDebug->end();

  loggerGPUDebug->begin("Render GUI " + std::to_string(globalFrame), currentFrame);
  // TODO: move to debug visualization
  int blurKernelSize = blur->getKernelSize();
  if (gui->drawInputInt("Bloom", {20, 480}, {{"Kernel", &blurKernelSize}})) {
    blur->setKernelSize(blurKernelSize);
  }
  int blurSigma = blur->getSigma();
  if (gui->drawInputInt("Bloom", {20, 480}, {{"Sigma", &blurSigma}})) {
    blur->setSigma(blurSigma);
  }
  int bloomPasses = settings->getBloomPasses();
  if (gui->drawInputInt("Bloom", {20, 480}, {{"Passes", &bloomPasses}})) {
    settings->setBloomPasses(bloomPasses);
  }

  gui->drawText("FPS", {20, 20}, {std::to_string(fps)});
  if (gui->drawInputInt("FPS", {20, 20}, {{"##current", &desiredFPS}})) {
    FPSChanged = true;
  }
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

void renderGraphic() {
  // record command buffer
  commandBuffer->beginCommands(currentFrame);
  loggerGPU->setCommandBufferName("Draw command buffer", currentFrame, commandBuffer);

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
  vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[currentFrame],
                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, imageMemoryBarrier.size(),
                       imageMemoryBarrier.data());

  /////////////////////////////////////////////////////////////////////////////////////////
  // render graphic
  /////////////////////////////////////////////////////////////////////////////////////////
  VkClearValue clearColor;
  clearColor.color = settings->getClearColor();
  std::vector<VkRenderingAttachmentInfo> colorAttachmentInfo(2);
  colorAttachmentInfo[0] = VkRenderingAttachmentInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = graphicTexture[currentFrame]->getImageView()->getImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearColor,
  };
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

  vkCmdBeginRendering(commandBuffer->getCommandBuffer()[currentFrame], &renderInfo);
  loggerGPU->begin("Render light " + std::to_string(globalFrame), currentFrame);
  lightManager->draw(currentFrame);
  loggerGPU->end();

  // draw scene here
  loggerGPU->begin("Render sprites " + std::to_string(globalFrame), currentFrame);
  spriteManager->draw(currentFrame, commandBuffer);
  loggerGPU->end();

  // wait model3D update
  if (updateJoints.valid()) {
    updateJoints.get();
  }

  loggerGPU->begin("Render models " + std::to_string(globalFrame), currentFrame);
  modelManager->draw(currentFrame, commandBuffer);
  loggerGPU->end();

  // submit model3D update
  updateJoints = pool->submit([&]() {
    loggerCPU->begin("Update animation " + std::to_string(globalFrame));
    // we want update model for next frame, current frame we can't touch and update because it will be used on GPU
    modelManager->updateAnimation(1 - currentFrame, frameTimer);
    loggerCPU->end();
  });

  loggerGPU->begin("Render terrain " + std::to_string(globalFrame), currentFrame);
  if (terrainWireframe)
    terrain->draw(currentFrame, commandBuffer, TerrainPipeline::WIREFRAME);
  else
    terrain->draw(currentFrame, commandBuffer, TerrainPipeline::FILL);
  if (terrainNormals) terrain->draw(currentFrame, commandBuffer, TerrainPipeline::NORMAL);
  loggerGPU->end();

  loggerGPU->begin("Render spheres " + std::to_string(globalFrame), currentFrame);
  for (auto sphere : spheres) {
    sphere->draw(currentFrame, commandBuffer);
  }
  loggerGPU->end();

  // contains transparency, should be drawn last
  loggerGPU->begin("Render particles " + std::to_string(globalFrame), currentFrame);
  particleSystem->drawGraphic(currentFrame, commandBuffer);
  loggerGPU->end();

  vkCmdEndRendering(commandBuffer->getCommandBuffer()[currentFrame]);
  commandBuffer->endCommands();
}

void initialize() {
  settings = std::make_shared<Settings>();
  settings->setName("Vulkan");
  settings->setResolution(std::tuple{1600, 900});
  // for HDR, it's not SRGB, it's linear
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
    auto graphicImageView = std::make_shared<ImageView>(graphicImage, VK_IMAGE_VIEW_TYPE_2D, 1, 0, 1,
                                                        VK_IMAGE_ASPECT_COLOR_BIT, state->getDevice());
    graphicTexture[i] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, graphicImageView,
                                                  state->getDevice());
    {
      auto blurImage = std::make_shared<Image>(settings->getResolution(), 1, 1, settings->getGraphicColorFormat(),
                                               VK_IMAGE_TILING_OPTIMAL,
                                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->getDevice());
      blurImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                              commandBufferTransfer);
      auto blurImageView = std::make_shared<ImageView>(blurImage, VK_IMAGE_VIEW_TYPE_2D, 1, 0, 1,
                                                       VK_IMAGE_ASPECT_COLOR_BIT, state->getDevice());
      blurTextureIn[i] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, blurImageView,
                                                   state->getDevice());
    }
    {
      auto blurImage = std::make_shared<Image>(settings->getResolution(), 1, 1, settings->getGraphicColorFormat(),
                                               VK_IMAGE_TILING_OPTIMAL,
                                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->getDevice());
      blurImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                              commandBufferTransfer);
      auto blurImageView = std::make_shared<ImageView>(blurImage, VK_IMAGE_VIEW_TYPE_2D, 1, 0, 1,
                                                       VK_IMAGE_ASPECT_COLOR_BIT, state->getDevice());
      blurTextureOut[i] = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, blurImageView,
                                                    state->getDevice());
    }
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

  spriteManager = std::make_shared<SpriteManager>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, lightManager,
      commandBufferTransfer, descriptorPool, device, settings);
  modelManager = std::make_shared<Model3DManager>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, lightManager,
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
                                                   VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, commandBufferTransfer, device);
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
  spheres[0]->setColor({0.f, 0.f, 0.1f, 1.f});
  spheres[0]->setCamera(camera);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -5.f, 0.f));
    spheres[0]->setModel(model);
  }
  spheres[1] = std::make_shared<Sphere>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, VK_CULL_MODE_BACK_BIT,
      VK_POLYGON_MODE_FILL, commandBufferTransfer, state);
  spheres[1]->setColor({0.f, 0.f, 0.5f, 1.f});
  spheres[1]->setCamera(camera);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(2.f, -5.f, 0.f));
    spheres[1]->setModel(model);
  }
  spheres[2] = std::make_shared<Sphere>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, VK_CULL_MODE_BACK_BIT,
      VK_POLYGON_MODE_FILL, commandBufferTransfer, state);
  spheres[2]->setColor({0.f, 0.f, 10.f, 1.f});
  spheres[2]->setCamera(camera);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(-2.f, -5.f, 0.f));
    spheres[2]->setModel(model);
  }
  // HDR
  spheres[3] = std::make_shared<Sphere>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, VK_CULL_MODE_BACK_BIT,
      VK_POLYGON_MODE_FILL, commandBufferTransfer, state);
  spheres[3]->setColor({5.f, 0.f, 0.f, 1.f});
  spheres[3]->setCamera(camera);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -5.f, 2.f));
    spheres[3]->setModel(model);
  }
  spheres[4] = std::make_shared<Sphere>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, VK_CULL_MODE_BACK_BIT,
      VK_POLYGON_MODE_FILL, commandBufferTransfer, state);
  spheres[4]->setColor({0.f, 5.f, 0.f, 1.f});
  spheres[4]->setCamera(camera);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -5.f, -2.f));
    spheres[4]->setModel(model);
  }
  spheres[5] = std::make_shared<Sphere>(
      std::vector{settings->getGraphicColorFormat(), settings->getGraphicColorFormat()}, VK_CULL_MODE_BACK_BIT,
      VK_POLYGON_MODE_FILL, commandBufferTransfer, state);
  spheres[5]->setColor({0.f, 0.f, 20.f, 1.f});
  spheres[5]->setCamera(camera);
  {
    auto model = glm::translate(glm::mat4(1.f), glm::vec3(-4.f, -5.f, 0.f));
    spheres[5]->setModel(model);
  }

  // for postprocessing descriptors GENERAL is needed
  swapchain->overrideImageLayout(VK_IMAGE_LAYOUT_GENERAL);
  postprocessing = std::make_shared<Postprocessing>(graphicTexture, blurTextureIn, swapchain->getImageViews(), state);
  // but we expect it to be in VK_IMAGE_LAYOUT_PRESENT_SRC_KHR as start value
  swapchain->changeImageLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, commandBufferTransfer);

  blur = std::make_shared<Blur>(blurTextureIn, blurTextureOut, state);
  debugVisualization->setPostprocessing(postprocessing);
  pool = std::make_shared<BS::thread_pool>(6);
}

void drawFrame() {
  currentFrame = globalFrame % settings->getMaxFramesInFlight();
  ////////////////////////////////////////////////////////////////////////////////////////
  // compute particles
  ////////////////////////////////////////////////////////////////////////////////////////
  std::vector<VkFence> waitFences = {inFlightFences[currentFrame]->getFence()};
  auto result = vkWaitForFences(device->getLogicalDevice(), waitFences.size(), waitFences.data(), VK_TRUE, UINT64_MAX);
  if (result != VK_SUCCESS) throw std::runtime_error("Can't wait for fence");

  result = vkResetFences(device->getLogicalDevice(), waitFences.size(), waitFences.data());
  if (result != VK_SUCCESS) throw std::runtime_error("Can't reset fence");

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
  submitInfo.pCommandBuffers = &commandBuffer->getCommandBuffer()[currentFrame];
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  commandBuffer->submitToQueue(submitInfo, nullptr);

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
}

void mainLoop() {
  auto startTimeFPS = std::chrono::high_resolution_clock::now();
  auto globalStartTimeFPS = std::chrono::high_resolution_clock::now();
  int frameFPS = 0;
  while (!glfwWindowShouldClose(window->getWindow())) {
    auto startTimeCurrent = std::chrono::high_resolution_clock::now();
    glfwPollEvents();
    drawFrame();
    if (FPSChanged) {
      FPSChanged = false;
      sleepFrame = 0;
      globalStartTimeFPS = std::chrono::high_resolution_clock::now();
    }
    globalFrame++;
    sleepFrame++;
    frameFPS++;
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsedSleep = std::chrono::duration_cast<std::chrono::milliseconds>(end - globalStartTimeFPS).count();
    uint64_t expected = (1000.f / desiredFPS) * sleepFrame;
    if (elapsedSleep < expected) {
      loggerCPU->begin("Sleep for: " + std::to_string(expected - elapsedSleep));
      std::this_thread::sleep_for(std::chrono::milliseconds(expected - elapsedSleep));
      loggerCPU->end();
    }
    // calculate FPS and model timings
    end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsedCurrent = end - startTimeCurrent;
    frameTimer = elapsedCurrent.count();
    std::chrono::duration<double> elapsed = end - startTimeFPS;
    // calculate frames per second
    if (elapsed.count() > 1.f) {
      fps = frameFPS;
      frameFPS = 0;
      startTimeFPS = std::chrono::high_resolution_clock::now();
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