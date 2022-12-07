#include "GUI.h"
#include "Sampler.h"
#include "Descriptor.h"

VertexBufferGUI::VertexBufferGUI(std::vector<VertexGUI> vertices,
                                 std::shared_ptr<CommandPool> commandPool,
                                 std::shared_ptr<Queue> queue,
                                 std::shared_ptr<Device> device) {
  VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

  auto stagingBuffer = std::make_shared<Buffer>(
      bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, device);

  void* data;
  vkMapMemory(device->getLogicalDevice(), stagingBuffer->getMemory(), 0, bufferSize, 0, &data);
  memcpy(data, vertices.data(), (size_t)bufferSize);
  vkUnmapMemory(device->getLogicalDevice(), stagingBuffer->getMemory());

  _buffer = std::make_shared<Buffer>(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, device);

  _buffer->copyFrom(stagingBuffer, commandPool, queue);
}

std::shared_ptr<Buffer> VertexBufferGUI::getBuffer() { return _buffer; }

GUI::GUI(std::tuple<int, int> resolution, std::shared_ptr<Device> device) {
  _device = device;
  _resolution = resolution;

  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.FontGlobalScale = _scale;
  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(_scale);
  // Color scheme
  style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
  style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
  style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
  style.Colors[ImGuiCol_Header] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
  style.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
  // Dimensions
  io.DisplaySize = ImVec2(std::get<0>(resolution), std::get<1>(resolution));
  io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
}

void GUI::initialize(std::shared_ptr<RenderPass> renderPass,
                     std::shared_ptr<Queue> queue,
                     std::shared_ptr<CommandPool> commandPool) {
  _commandPool = commandPool;
  _queue = queue;

  ImGuiIO& io = ImGui::GetIO();

  // Create font texture
  unsigned char* fontData;
  int texWidth, texHeight;
  io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
  VkDeviceSize uploadSize = texWidth * texHeight * 4 * sizeof(char);

  // check if device extensions are supported
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(_device->getPhysicalDevice(), nullptr, &extensionCount, nullptr);

  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(_device->getPhysicalDevice(), nullptr, &extensionCount,
                                       availableExtensions.data());
  bool supported = false;
  for (auto extension : availableExtensions) {
    if (extension.extensionName == VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME) {
      supported = true;
      break;
    }
  }

  // SRS - Get Vulkan device driver information if available, use later for display
  if (supported) {
    VkPhysicalDeviceProperties2 deviceProperties2 = {};
    deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProperties2.pNext = &_driverProperties;
    _driverProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
    vkGetPhysicalDeviceProperties2(_device->getPhysicalDevice(), &deviceProperties2);
  }

  _fontImage = std::make_shared<Image>(
      std::tuple{texWidth, texHeight}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _device);
  auto imageView = std::make_shared<ImageView>(_fontImage, VK_IMAGE_ASPECT_COLOR_BIT, _device);
  auto buffer = std::make_shared<Buffer>(uploadSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                         _device);

  void* data;
  vkMapMemory(_device->getLogicalDevice(), buffer->getMemory(), 0, uploadSize, 0, &data);
  memcpy(data, fontData, uploadSize);
  vkUnmapMemory(_device->getLogicalDevice(), buffer->getMemory());

  _fontImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandPool, queue);
  _fontImage->copyFrom(buffer, commandPool, queue);
  _fontImage->changeLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandPool,
                           queue);
  _fontTexture = std::make_shared<Texture>(imageView, _device);

  auto descriptorPool = std::make_shared<DescriptorPool>(4, _device);
  auto descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_device);
  descriptorSetLayout->createGraphic();
  _descriptorSet = std::make_shared<DescriptorSet>(2, descriptorSetLayout, descriptorPool, _device);
  _uniformBuffer = std::make_shared<UniformBuffer>(2, sizeof(UniformData), commandPool, queue, _device);

  _descriptorSet->createGraphic(_fontTexture, _uniformBuffer);

  auto shader = std::make_shared<Shader>(_device);
  shader->add("../shaders/ui_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader->add("../shaders/ui_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

  _pipeline = std::make_shared<Pipeline>(shader, descriptorSetLayout, _device);
  _pipeline->createGraphic(renderPass);
}

void GUI::newFrame(bool updateFrameGraph) {
  ImGui::NewFrame();
  // Init imGui windows and elements
  // SRS - Set initial position of default Debug window (note: Debug window sets its own initial size, use
  // ImGuiSetCond_Always to override)
  ImGui::SetWindowPos(ImVec2(20 * _scale, 20 * _scale), ImGuiCond_FirstUseEver);
  ImGui::SetWindowSize(ImVec2(300 * _scale, 300 * _scale), ImGuiCond_Always);
  ImGui::TextUnformatted("Title");
  ImGui::TextUnformatted("Device name");

  // Update frame time display
  if (updateFrameGraph) {
    std::rotate(_frameTimes.begin(), _frameTimes.begin() + 1, _frameTimes.end());
    float frameTime = 1000.0f / (_frameTimer * 1000.0f);
    _frameTimes.back() = frameTime;
    if (frameTime < _frameTimeMin) {
      _frameTimeMin = frameTime;
    }
    if (frameTime > _frameTimeMax) {
      _frameTimeMax = frameTime;
    }
  }

  ImGui::PlotLines("Frame Times", &_frameTimes[0], 50, 0, "", _frameTimeMin, _frameTimeMax, ImVec2(0, 80));

  ImGui::End();

  // SRS - ShowDemoWindow() sets its own initial position and size, cannot override here
  ImGui::ShowDemoWindow();

  // Render to generate draw buffers
  ImGui::Render();
}

void GUI::updateBuffers() {
  ImDrawData* imDrawData = ImGui::GetDrawData();

  // Note: Alignment is done inside buffer creation
  VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
  VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

  if ((vertexBufferSize == 0) || (indexBufferSize == 0)) {
    return;
  }

  // Update buffers only if vertex or index count has been changed compared to current buffer size
  // Vertex buffer
  // Upload data
  std::vector<VertexGUI> vtxDst;
  std::vector<uint32_t> idxDst;

  for (int n = 0; n < imDrawData->CmdListsCount; n++) {
    const ImDrawList* cmd_list = imDrawData->CmdLists[n];
    for (int k = 0; k < cmd_list->VtxBuffer.Size; k++) {
      glm::vec2 pos = {cmd_list->VtxBuffer.Data[k].pos.x, cmd_list->VtxBuffer.Data[k].pos.y};
      glm::vec2 uv = {cmd_list->VtxBuffer.Data[k].uv.x, cmd_list->VtxBuffer.Data[k].uv.y};
      char* bytepointer = reinterpret_cast<char*>(&cmd_list->VtxBuffer.Data[k].col);
      glm::vec4 color;
      color.r = static_cast<int>(bytepointer[0]);
      color.g = static_cast<int>(bytepointer[1]);
      color.b = static_cast<int>(bytepointer[2]);
      color.a = static_cast<int>(bytepointer[3]);
      vtxDst.push_back(VertexGUI{
          .pos = pos,
          .texCoord = uv,
          .color = color,
      });
    }
    for (int k = 0; k < cmd_list->IdxBuffer.Size; k++) {
      idxDst.push_back(cmd_list->IdxBuffer.Data[k]);
    }
  }

  if ((_vertexBuffer == nullptr) || (_vertexCount != imDrawData->TotalVtxCount)) {
    _vertexBuffer = std::make_shared<VertexBufferGUI>(vtxDst, _commandPool, _queue, _device);
    _vertexCount = imDrawData->TotalVtxCount;
  }

  // Index buffer
  if ((_indexBuffer == nullptr) || (_indexCount < imDrawData->TotalIdxCount)) {
    _indexBuffer = std::make_shared<IndexBuffer>(idxDst, _commandPool, _queue, _device);
    _indexCount = imDrawData->TotalIdxCount;
  }
}

void GUI::drawFrame(int current, VkCommandBuffer commandBuffer) {
  ImGuiIO& io = ImGui::GetIO();

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline->getPipelineLayout(), 0, 1,
                          &_descriptorSet->getDescriptorSets()[current], 0, nullptr);
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline->getPipeline());

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  std::tie(viewport.width, viewport.height) = _resolution;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  // UI scale and translate via push constants
  uniformData.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
  uniformData.translate = glm::vec2(-1.0f);

  void* data;
  vkMapMemory(_device->getLogicalDevice(), _uniformBuffer->getBuffer()[current]->getMemory(), 0, sizeof(uniformData), 0,
              &data);
  memcpy(data, &uniformData, sizeof(uniformData));
  vkUnmapMemory(_device->getLogicalDevice(), _uniformBuffer->getBuffer()[current]->getMemory());

  // Render commands
  ImDrawData* imDrawData = ImGui::GetDrawData();
  int32_t vertexOffset = 0;
  int32_t indexOffset = 0;

  if (imDrawData->CmdListsCount > 0) {
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &_vertexBuffer->getBuffer()->getData(), offsets);
    vkCmdBindIndexBuffer(commandBuffer, _indexBuffer->getBuffer()->getData(), 0, VK_INDEX_TYPE_UINT16);

    for (int32_t i = 0; i < imDrawData->CmdListsCount; i++) {
      const ImDrawList* cmd_list = imDrawData->CmdLists[i];
      for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++) {
        const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
        VkRect2D scissorRect;
        scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
        scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
        scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
        scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);
        vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
        indexOffset += pcmd->ElemCount;
      }
      vertexOffset += cmd_list->VtxBuffer.Size;
    }
  }
}

GUI::~GUI() {}