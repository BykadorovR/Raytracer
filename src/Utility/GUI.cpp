#include "GUI.h"
#include "Sampler.h"
#include "Descriptor.h"

std::tuple<float, float> mousePos;
bool mouseDownLeft = false;
bool mouseDownRight = false;

void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) { mousePos = {xpos, ypos}; }

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
    mouseDownLeft = true;
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
    mouseDownRight = true;
  }
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
    mouseDownLeft = false;
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
    mouseDownRight = false;
  }
}

GUI::GUI(std::tuple<int, int> resolution, std::shared_ptr<Window> window, std::shared_ptr<Device> device) {
  _device = device;
  _window = window;
  _resolution = resolution;

  glfwSetCursorPosCallback(_window->getWindow(), cursorPositionCallback);
  glfwSetMouseButtonCallback(_window->getWindow(), mouseButtonCallback);

  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
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
  for (const auto& extension : availableExtensions) {
    if (std::string(extension.extensionName) == std::string(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME)) {
      supported = true;
      break;
    }
  }

  auto stagingBuffer = std::make_shared<Buffer>(
      uploadSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _device);

  void* data;
  vkMapMemory(_device->getLogicalDevice(), stagingBuffer->getMemory(), 0, uploadSize, 0, &data);
  memcpy(data, fontData, uploadSize);
  vkUnmapMemory(_device->getLogicalDevice(), stagingBuffer->getMemory());

  _fontImage = std::make_shared<Image>(
      std::tuple{texWidth, texHeight}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _device);
  _fontImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandPool, queue);
  _fontImage->copyFrom(stagingBuffer, commandPool, queue);
  _fontImage->changeLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, commandPool, queue);
  _imageView = std::make_shared<ImageView>(_fontImage, VK_IMAGE_ASPECT_COLOR_BIT, _device);
  _fontTexture = std::make_shared<Texture>(_imageView, _device);

  _descriptorPool = std::make_shared<DescriptorPool>(100, _device);
  _descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_device);
  _descriptorSetLayout->createGUI();

  _uniformBuffer = std::make_shared<UniformBuffer>(2, sizeof(UniformData), commandPool, queue, _device);
  _descriptorSet = std::make_shared<DescriptorSet>(2, _descriptorSetLayout, _descriptorPool, _device);
  _descriptorSet->createGUI(_fontTexture, _uniformBuffer);

  auto shader = std::make_shared<Shader>(_device);
  shader->add("../shaders/ui_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader->add("../shaders/ui_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

  _pipeline = std::make_shared<Pipeline>(shader, _descriptorSetLayout, _device);
  _pipeline->createGUI(VertexGUI::getBindingDescription(), VertexGUI::getAttributeDescriptions(), renderPass);
}

void GUI::newFrame() {
  ImGui::NewFrame();
  // Init imGui windows and elements
  // SRS - Set initial position of default Debug window (note: Debug window sets its own initial size, use
  // ImGuiSetCond_Always to override)
  ImGui::SetWindowPos(ImVec2(20 * _scale, 20 * _scale), ImGuiCond_FirstUseEver);
  ImGui::SetWindowSize(ImVec2(150 * _scale, 60 * _scale), ImGuiCond_Always);
  ImGui::Text(std::string("FPS " + std::to_string(_fps)).c_str());

  // Render to generate draw buffers
  ImGui::Render();
}

void GUI::updateBuffers(int current) {
  ImDrawData* imDrawData = ImGui::GetDrawData();

  // Note: Alignment is done inside buffer creation
  VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
  VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

  if ((vertexBufferSize == 0) || (indexBufferSize == 0)) {
    return;
  }

  if ((_vertexBuffer[current] == nullptr) || (_vertexCount[current] != imDrawData->TotalVtxCount)) {
    _vertexBuffer[current] = std::make_shared<Buffer>(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, _device);
    _vertexCount[current] = imDrawData->TotalVtxCount;
    _vertexBuffer[current]->map();
  }

  // Index buffer
  if ((_indexBuffer[current] == nullptr) || (_indexCount[current] != imDrawData->TotalIdxCount)) {
    _indexBuffer[current] = std::make_shared<Buffer>(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, _device);
    _indexCount[current] = imDrawData->TotalIdxCount;
    _indexBuffer[current]->map();
  }

  // Upload data
  ImDrawVert* vtxDst = (ImDrawVert*)_vertexBuffer[current]->getMappedMemory();
  ImDrawIdx* idxDst = (ImDrawIdx*)_indexBuffer[current]->getMappedMemory();

  for (int n = 0; n < imDrawData->CmdListsCount; n++) {
    const ImDrawList* cmd_list = imDrawData->CmdLists[n];
    memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
    memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
    vtxDst += cmd_list->VtxBuffer.Size;
    idxDst += cmd_list->IdxBuffer.Size;
  }

  _vertexBuffer[current]->flush();
  _indexBuffer[current]->flush();
}

void GUI::setFPS(float fps) { _fps = fps; }

void GUI::drawFrame(int current, VkCommandBuffer commandBuffer) {
  ImGuiIO& io = ImGui::GetIO();

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline->getPipeline());

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  std::tie(viewport.width, viewport.height) = _resolution;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  // UI scale and translate via push constants
  UniformData uniformData{};
  uniformData.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
  uniformData.translate = glm::vec2(-1.0f);

  void* data;
  vkMapMemory(_device->getLogicalDevice(), _uniformBuffer->getBuffer()[current]->getMemory(), 0, sizeof(uniformData), 0,
              &data);
  memcpy(data, &uniformData, sizeof(uniformData));
  vkUnmapMemory(_device->getLogicalDevice(), _uniformBuffer->getBuffer()[current]->getMemory());

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline->getPipelineLayout(), 0, 1,
                          &_descriptorSet->getDescriptorSets()[current], 0, nullptr);

  // Render commands
  ImDrawData* imDrawData = ImGui::GetDrawData();
  int32_t vertexOffset = 0;
  int32_t indexOffset = 0;

  if (imDrawData->CmdListsCount > 0) {
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &_vertexBuffer[current]->getData(), offsets);
    vkCmdBindIndexBuffer(commandBuffer, _indexBuffer[current]->getData(), 0, VK_INDEX_TYPE_UINT16);

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

  io.MousePos = ImVec2(std::get<0>(mousePos), std::get<1>(mousePos));
  io.MouseDown[0] = mouseDownLeft;
  io.MouseDown[1] = mouseDownRight;
}

GUI::~GUI() { ImGui::DestroyContext(); }