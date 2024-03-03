module Equirectangular;
import Shader;

namespace VulkanEngine {
Equirectangular::Equirectangular(std::string path,
                                 std::shared_ptr<CommandBuffer> commandBufferTransfer,
                                 std::shared_ptr<ResourceManager> resourceManager,
                                 std::shared_ptr<State> state) {
  _commandBufferTransfer = commandBufferTransfer;
  _state = state;

  float* pixels;
  int texWidth, texHeight, texChannels;
  pixels = stbi_loadf(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
  if (!pixels) {
    throw std::runtime_error("failed to load texture image!");
  }

  int imageSize = texWidth * texHeight * STBI_rgb_alpha;
  int bufferSize = imageSize * sizeof(float);
  // fill buffer
  _stagingBuffer = std::make_shared<Buffer>(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                            state);
  void* data;
  vkMapMemory(state->getDevice()->getLogicalDevice(), _stagingBuffer->getMemory(), 0, bufferSize, 0, &data);
  memcpy((stbi_uc*)data, pixels, static_cast<size_t>(bufferSize));
  vkUnmapMemory(state->getDevice()->getLogicalDevice(), _stagingBuffer->getMemory());
  stbi_image_free(pixels);

  // image
  auto [width, height] = state->getSettings()->getResolution();
  _image = std::make_shared<Image>(
      std::tuple{texWidth, texHeight}, 1, 1, state->getSettings()->getGraphicColorFormat(), VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state);
  _image->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,
                       commandBufferTransfer);
  _image->copyFrom(_stagingBuffer, {0}, commandBufferTransfer);
  _image->changeLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, commandBufferTransfer);
  _imageView = std::make_shared<ImageView>(_image, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT, state);
  _texture = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 1, _imageView, state);

  // convert to cubemap
  _mesh3D = std::make_shared<Mesh3D>(state);

  std::vector<Vertex3D> vertices(8);
  vertices[0].pos = glm::vec3(-0.5, -0.5, 0.5);   // 0
  vertices[1].pos = glm::vec3(0.5, -0.5, 0.5);    // 1
  vertices[2].pos = glm::vec3(-0.5, 0.5, 0.5);    // 2
  vertices[3].pos = glm::vec3(0.5, 0.5, 0.5);     // 3
  vertices[4].pos = glm::vec3(-0.5, -0.5, -0.5);  // 4
  vertices[5].pos = glm::vec3(0.5, -0.5, -0.5);   // 5
  vertices[6].pos = glm::vec3(-0.5, 0.5, -0.5);   // 6
  vertices[7].pos = glm::vec3(0.5, 0.5, -0.5);    // 7

  std::vector<uint32_t> indices{                    // Bottom
                                0, 4, 5, 5, 1, 0,   // ccw if look to this face from down
                                                    //  Top
                                2, 3, 7, 7, 6, 2,   // ccw if look to this face from up
                                                    //  Left
                                0, 2, 6, 6, 4, 0,   // ccw if look to this face from left
                                                    //  Right
                                1, 5, 7, 7, 3, 1,   // ccw if look to this face from right
                                                    //  Front
                                1, 3, 2, 2, 0, 1,   // ccw if look to this face from front
                                                    //  Back
                                5, 4, 6, 6, 7, 5};  // ccw if look to this face from back
  _mesh3D->setVertices(vertices, commandBufferTransfer);
  _mesh3D->setIndexes(indices, commandBufferTransfer);
  _mesh3D->setColor(std::vector<glm::vec3>(vertices.size(), glm::vec3(1.f, 1.f, 1.f)), commandBufferTransfer);
  _cubemap = std::make_shared<Cubemap>(
      _state->getSettings()->getDepthResolution(), _state->getSettings()->getGraphicColorFormat(), 1,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, commandBufferTransfer, state);

  auto cameraLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
  cameraLayout->createUniformBuffer();
  // initialize camera UBO and descriptor sets for draw
  // initialize UBO
  _cameraBufferCubemap.resize(6);
  for (int i = 0; i < 6; i++) {
    _cameraBufferCubemap[i] = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(),
                                                              sizeof(BufferMVP), state);
  }
  _descriptorSetCameraCubemap.resize(6);
  for (int i = 0; i < 6; i++) {
    _descriptorSetCameraCubemap[i] = std::make_shared<DescriptorSet>(
        state->getSettings()->getMaxFramesInFlight(), cameraLayout, state->getDescriptorPool(), state->getDevice());
    _descriptorSetCameraCubemap[i]->createUniformBuffer(_cameraBufferCubemap[i]);
  }

  _material = std::make_shared<MaterialColor>(MaterialTarget::SIMPLE, commandBufferTransfer, state);
  _material->setBaseColor({_texture});
  {
    auto shader = std::make_shared<Shader>(state->getDevice());
    shader->add("shaders/skyboxEquirectangular_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader->add("shaders/skyboxEquirectangular_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    _pipelineEquirectangular = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
    _pipelineEquirectangular->createGraphic3D(
        std::vector{_state->getSettings()->getGraphicColorFormat()}, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL,
        {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
         shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
        {std::pair{std::string("camera"), cameraLayout},
         std::pair{std::string("texture"), _material->getDescriptorSetLayoutTextures()}},
        {}, _mesh3D->getBindingDescription(), _mesh3D->getAttributeDescriptions());
  }

  _loggerGPU = std::make_shared<VulkanEngine::LoggerGPU>(state);

  _camera = std::make_shared<CameraFly>(_state->getSettings());
  _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, 1.f, 0.f));
  _camera->setProjectionParameters(90.f, 0.1f, 100.f);
  _camera->setAspect(1.f);
}

std::shared_ptr<Cubemap> Equirectangular::convertToCubemap(std::shared_ptr<CommandBuffer> commandBuffer) {
  auto currentFrame = _state->getFrameInFlight();
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipelineEquirectangular->getPipeline());
  auto [width, height] = _state->getSettings()->getDepthResolution();
  // render equirectangular to cubemap
  /////////////////////////////////////////////////////////////////////////////////////////
  // render graphic
  /////////////////////////////////////////////////////////////////////////////////////////
  for (int i = 0; i < 6; i++) {
    auto currentTexture = _cubemap->getTextureSeparate()[i][0];
    VkClearValue clearColor;
    clearColor.color = _state->getSettings()->getClearColor();
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

    vkCmdBeginRendering(commandBuffer->getCommandBuffer()[currentFrame], &renderInfo);

    _loggerGPU->setCommandBufferName("Equirectangular", commandBuffer);
    _loggerGPU->begin("Render equirectangular");
    // up is inverted for X and Z because of some specific cubemap Y coordinate stuff
    switch (i) {
      case 0:
        // POSITIVE_X / right
        _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(1.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f));
        break;
      case 1:
        // NEGATIVE_X /left
        _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(-1.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f));
        break;
      case 2:
        // POSITIVE_Y / top
        _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
        break;
      case 3:
        // NEGATIVE_Y / bottom
        _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f), glm::vec3(0.f, 0.f, -1.f));
        break;
      case 4:
        // POSITIVE_Z / near
        _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f), glm::vec3(0.f, -1.f, 0.f));
        break;
      case 5:
        // NEGATIVE_Z / far
        _camera->setViewParameters(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, -1.f, 0.f));
        break;
    }
    VkViewport viewport{};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = VkExtent2D(width, height);
    vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);
    BufferMVP cameraUBO{};
    cameraUBO.model = glm::mat4(1.f);
    cameraUBO.view = _camera->getView();
    cameraUBO.projection = _camera->getProjection();

    void* data;
    vkMapMemory(_state->getDevice()->getLogicalDevice(),
                _cameraBufferCubemap[i]->getBuffer()[currentFrame]->getMemory(), 0, sizeof(cameraUBO), 0, &data);
    memcpy(data, &cameraUBO, sizeof(cameraUBO));
    vkUnmapMemory(_state->getDevice()->getLogicalDevice(),
                  _cameraBufferCubemap[i]->getBuffer()[currentFrame]->getMemory());

    VkBuffer vertexBuffers[] = {_mesh3D->getVertexBuffer()->getBuffer()->getData()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame],
                         _mesh3D->getIndexBuffer()->getBuffer()->getData(), 0, VK_INDEX_TYPE_UINT32);

    auto pipelineLayout = _pipelineEquirectangular->getDescriptorSetLayout();
    auto cameraLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                     [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                       return info.first == std::string("camera");
                                     });
    if (cameraLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              _pipelineEquirectangular->getPipelineLayout(), 0, 1,
                              &_descriptorSetCameraCubemap[i]->getDescriptorSets()[currentFrame], 0, nullptr);
    }

    auto textureLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                        return info.first == std::string("texture");
                                      });
    if (textureLayout != pipelineLayout.end()) {
      vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              _pipelineEquirectangular->getPipelineLayout(), 1, 1,
                              &_material->getDescriptorSetTextures(currentFrame)->getDescriptorSets()[currentFrame], 0,
                              nullptr);
    }

    vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame],
                     static_cast<uint32_t>(_mesh3D->getIndexData().size()), 1, 0, 0, 0);
    _loggerGPU->end();

    vkCmdEndRendering(commandBuffer->getCommandBuffer()[currentFrame]);

    VkImageMemoryBarrier colorBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                      .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                      .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                      .image = currentTexture->getImageView()->getImage()->getImage(),
                                      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                           .baseMipLevel = 0,
                                                           .levelCount = 1,
                                                           .baseArrayLayer = 0,
                                                           .layerCount = 1}};
    vkCmdPipelineBarrier(commandBuffer->getCommandBuffer()[currentFrame],
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // srcStageMask
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,          // dstStageMask
                         0, 0, nullptr, 0, nullptr,
                         1,             // imageMemoryBarrierCount
                         &colorBarrier  // pImageMemoryBarriers
    );
  }

  return _cubemap;
}

std::shared_ptr<Texture> Equirectangular::getTexture() { return _texture; }
}  // namespace VulkanEngine