#include "Utility/GUI.h"

GUI::GUI(std::shared_ptr<EngineState> engineState) {
  _engineState = engineState;
  _resolution = engineState->getSettings()->getResolution();

  _vertexBuffer.resize(engineState->getSettings()->getMaxFramesInFlight());
  _indexBuffer.resize(engineState->getSettings()->getMaxFramesInFlight());
  _vertexCount.resize(engineState->getSettings()->getMaxFramesInFlight(), 0);
  _indexCount.resize(engineState->getSettings()->getMaxFramesInFlight(), 0);

  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;

  ImGuiStyle& style = ImGui::GetStyle();
  // Color scheme
  style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
  style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
  style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
  style.Colors[ImGuiCol_Header] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
  style.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
  // Dimensions
  io.DisplaySize = ImVec2(std::get<0>(_resolution), std::get<1>(_resolution));
  io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
}

void GUI::reset() {
  ImGuiIO& io = ImGui::GetIO();
  _resolution = _engineState->getSettings()->getResolution();
  io.DisplaySize = ImVec2(std::get<0>(_resolution), std::get<1>(_resolution));
}

void GUI::initialize(std::shared_ptr<CommandBuffer> commandBufferTransfer) {
  ImGuiIO& io = ImGui::GetIO();

  // Create font texture
  unsigned char* fontData;
  int texWidth, texHeight;
  io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
  VkDeviceSize uploadSize = texWidth * texHeight * 4 * sizeof(char);

  // check if device extensions are supported
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(_engineState->getDevice()->getPhysicalDevice(), nullptr, &extensionCount,
                                       nullptr);

  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(_engineState->getDevice()->getPhysicalDevice(), nullptr, &extensionCount,
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
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);

  stagingBuffer->setData(fontData);

  _fontImage = std::make_shared<Image>(
      std::tuple{texWidth, texHeight}, 1, 1, _engineState->getSettings()->getLoadTextureColorFormat(),
      VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _engineState);
  _fontImage->changeLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
                           1, 1, commandBufferTransfer);
  _fontImage->copyFrom(stagingBuffer, {0}, commandBufferTransfer);
  _fontImage->changeLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1,
                           1, commandBufferTransfer);
  _imageView = std::make_shared<ImageView>(_fontImage, VK_IMAGE_VIEW_TYPE_2D, 0, 1, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT,
                                           _engineState);
  _fontTexture = std::make_shared<Texture>(VK_SAMPLER_ADDRESS_MODE_REPEAT, 1, VK_FILTER_LINEAR, _imageView,
                                           _engineState);

  _descriptorPool = std::make_shared<DescriptorPool>(100, _engineState->getDevice());
  _descriptorSetLayout = std::make_shared<DescriptorSetLayout>(_engineState->getDevice());
  _descriptorSetLayout->createGUI();

  _uniformBuffer.resize(_engineState->getSettings()->getMaxFramesInFlight());
  for (int i = 0; i < _engineState->getSettings()->getMaxFramesInFlight(); i++)
    _uniformBuffer[i] = std::make_shared<Buffer>(
        sizeof(UniformData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _engineState);
  _descriptorSet = std::make_shared<DescriptorSet>(_engineState->getSettings()->getMaxFramesInFlight(),
                                                   _descriptorSetLayout, _descriptorPool, _engineState->getDevice());
  _descriptorSet->createGUI(_fontTexture, _uniformBuffer);

  auto shader = std::make_shared<Shader>(_engineState);
  shader->add("shaders/UI/ui_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader->add("shaders/UI/ui_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  _renderPass = std::make_shared<RenderPass>(_engineState->getSettings(), _engineState->getDevice());
  _renderPass->initializeDebug();
  _pipeline = std::make_shared<Pipeline>(_engineState->getSettings(), _engineState->getDevice());
  _pipeline->createHUD({shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                        shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                       {{"gui", _descriptorSetLayout}}, {}, VertexGUI::getBindingDescription(),
                       VertexGUI::getAttributeDescriptions(), _renderPass);
  ImGui::NewFrame();
}

bool GUI::drawListBox(std::vector<std::string> list, std::map<std::string, int*> variable, int displayedNumber) {
  bool result = false;

  for (auto& [key, value] : variable) {
    std::vector<const char*> listFormatted;
    for (auto& item : list) {
      listFormatted.push_back(item.c_str());
    }
    if (ImGui::ListBox(key.c_str(), value, listFormatted.data(), listFormatted.size(), displayedNumber)) result = true;
  }

  return result;
}

bool GUI::drawButton(std::string label, bool hideWindow) {
  bool result = false;

  ImGuiWindowFlags flags = 0;
  if (hideWindow) {
    ImGui::SetNextWindowBgAlpha(0.f);
    flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground;
  }
  if (ImGui::Button(label.c_str())) {
    result = true;
  }
  return result;
}

bool GUI::drawCheckbox(std::map<std::string, bool*> variable) {
  bool result = false;

  for (auto& [key, value] : variable) {
    if (ImGui::Checkbox(key.c_str(), value)) result = true;
  }

  return result;
}

bool GUI::drawSlider(std::map<std::string, float*> variable, std::map<std::string, std::tuple<float, float>> range) {
  bool result = false;
  for (auto& [key, value] : variable) {
    if (ImGui::SliderFloat(key.c_str(), value, std::get<0>(range[key]), std::get<1>(range[key]), "%.2f",
                           ImGuiSliderFlags_AlwaysClamp))
      result = true;
  }
  return result;
}

bool GUI::drawInputFloat(std::map<std::string, float*> variable) {
  bool result = false;
  for (auto& [key, value] : variable) {
    ImGui::PushItemWidth(100);
    if (ImGui::InputFloat(key.c_str(), value, 0.01f, 1.f, "%.2f")) result = true;
    ImGui::PopItemWidth();
  }

  return result;
}

bool GUI::drawInputText(std::string label, char* buffer, int size) {
  bool result = false;
  if (ImGui::InputText(label.c_str(), buffer, size, ImGuiInputTextFlags_EnterReturnsTrue)) result = true;

  return result;
}

bool GUI::drawInputInt(std::map<std::string, int*> variable) {
  bool result = false;
  for (auto& [key, value] : variable) {
    ImGui::PushItemWidth(100);
    if (ImGui::InputInt(key.c_str(), value, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)) result = true;
    ImGui::PopItemWidth();
  }

  return result;
}

void GUI::drawText(std::vector<std::string> text) {
  for (auto value : text) {
    ImGui::Text("%s", value.c_str());
  }
}

void GUI::updateBuffers(int current) {
  ImGui::Render();
  ImDrawData* imDrawData = ImGui::GetDrawData();

  // Note: Alignment is done inside buffer creation
  VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
  VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

  if ((vertexBufferSize == 0) || (indexBufferSize == 0)) {
    return;
  }

  if ((_vertexBuffer[current] == nullptr) || (_vertexCount[current] != imDrawData->TotalVtxCount)) {
    _vertexBuffer[current] = std::make_shared<Buffer>(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, _engineState);
    _vertexCount[current] = imDrawData->TotalVtxCount;
  }

  // Index buffer
  if ((_indexBuffer[current] == nullptr) || (_indexCount[current] != imDrawData->TotalIdxCount)) {
    _indexBuffer[current] = std::make_shared<Buffer>(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, _engineState);
    _indexCount[current] = imDrawData->TotalIdxCount;
  }

  // Upload data
  int vertexDst = 0;
  int indexDst = 0;
  for (int n = 0; n < imDrawData->CmdListsCount; n++) {
    const ImDrawList* cmdList = imDrawData->CmdLists[n];
    _vertexBuffer[current]->setData(cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert), vertexDst);
    _indexBuffer[current]->setData(cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx), indexDst);
    vertexDst += cmdList->VtxBuffer.Size * sizeof(ImDrawVert);
    indexDst += cmdList->IdxBuffer.Size * sizeof(ImDrawIdx);
  }
}

void GUI::drawFrame(int current, std::shared_ptr<CommandBuffer> commandBuffer) {
  ImGuiIO& io = ImGui::GetIO();

  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[current], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline->getPipeline());

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  std::tie(viewport.width, viewport.height) = _resolution;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer->getCommandBuffer()[current], 0, 1, &viewport);

  // UI scale and translate via push constants
  UniformData uniformData{};
  uniformData.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
  uniformData.translate = glm::vec2(-1.0f);

  _uniformBuffer[current]->setData(&uniformData);

  vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[current], VK_PIPELINE_BIND_POINT_GRAPHICS,
                          _pipeline->getPipelineLayout(), 0, 1, &_descriptorSet->getDescriptorSets()[current], 0,
                          nullptr);

  // Render commands
  ImDrawData* imDrawData = ImGui::GetDrawData();
  int32_t vertexOffset = 0;
  int32_t indexOffset = 0;

  if (imDrawData->CmdListsCount > 0) {
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[current], 0, 1, &_vertexBuffer[current]->getData(),
                           offsets);
    vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[current], _indexBuffer[current]->getData(), 0,
                         VK_INDEX_TYPE_UINT16);

    for (int32_t i = 0; i < imDrawData->CmdListsCount; i++) {
      const ImDrawList* cmd_list = imDrawData->CmdLists[i];
      for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++) {
        const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
        VkRect2D scissorRect;
        scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
        scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
        scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
        scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
        vkCmdSetScissor(commandBuffer->getCommandBuffer()[current], 0, 1, &scissorRect);
        vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[current], pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
        indexOffset += pcmd->ElemCount;
      }
      vertexOffset += cmd_list->VtxBuffer.Size;
    }
  }

  ImGui::NewFrame();
}

void GUI::startWindow(std::string name, float fontScale) {
  ImGui::Begin(name.c_str(), 0, ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::GetFont()->Scale = fontScale;
  ImGui::PushFont(ImGui::GetFont());
}

void GUI::setWindowSize(std::tuple<int, int> size) {
  ImGui::SetWindowSize(ImVec2(std::get<0>(size), std::get<1>(size)));
}

void GUI::setWindowPosition(std::tuple<int, int> position) {
  ImGui::SetWindowPos(ImVec2(std::get<0>(position), std::get<1>(position)));
}

std::tuple<int, int> GUI::getWindowSize() {
  auto size = ImGui::GetWindowSize();
  return {size.x, size.y};
}

std::tuple<int, int> GUI::getWindowPosition() {
  auto position = ImGui::GetWindowPos();
  return {position.x, position.y};
}

void GUI::endWindow() {
  ImGui::PopFont();
  ImGui::End();
}

bool GUI::startTree(std::string name, bool open) {
  ImGui::SetNextItemOpen(open, ImGuiCond_FirstUseEver);
  return ImGui::TreeNode(name.c_str());
}

void GUI::endTree() { ImGui::TreePop(); }

GUI::~GUI() { ImGui::DestroyContext(); }

bool GUI::cursorNotify(float xPos, float yPos) {
  ImGuiIO& io = ImGui::GetIO();
  io.MousePos = ImVec2(xPos, yPos);

  if (io.WantCaptureMouse) {
    return true;
  }

  return false;
}

bool GUI::mouseNotify(int button, int action, int mods) {
  ImGuiIO& io = ImGui::GetIO();
#ifdef __ANDROID__
  if (action == 1) io.MouseDown[0] = true;
  if (action == 0) io.MouseDown[0] = false;
#else
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
    io.MouseDown[0] = true;
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
    io.MouseDown[1] = true;
  }
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
    io.MouseDown[0] = false;
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
    io.MouseDown[1] = false;
  }
#endif
  if (io.WantCaptureMouse) {
    return true;
  }

  return false;
}

bool GUI::charNotify(unsigned int code) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddInputCharacter(code);

  if (io.WantCaptureKeyboard) {
    return true;
  }

  return false;
}

bool GUI::scrollNotify(double xOffset, double yOffset) {
  ImGuiIO& io = ImGui::GetIO();
  io.MouseWheelH += (float)xOffset;
  io.MouseWheel += (float)yOffset;

  if (io.WantCaptureMouse) {
    return true;
  }

  return false;
}

bool GUI::keyNotify(int key, int scancode, int action, int mods) {
#ifndef __ANDROID__
  ImGuiIO& io = ImGui::GetIO();
  if (key == GLFW_KEY_BACKSPACE && action == GLFW_PRESS) {
    io.AddKeyEvent(ImGuiKey_Backspace, true);
    io.AddKeyEvent(ImGuiKey_Backspace, false);
  }
  if (key == GLFW_KEY_DELETE && action == GLFW_PRESS) {
    io.AddKeyEvent(ImGuiKey_Delete, true);
    io.AddKeyEvent(ImGuiKey_Delete, false);
  }
  if (key == GLFW_KEY_LEFT && action == GLFW_PRESS) {
    io.AddKeyEvent(ImGuiKey_LeftArrow, true);
    io.AddKeyEvent(ImGuiKey_LeftArrow, false);
  }
  if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) {
    io.AddKeyEvent(ImGuiKey_RightArrow, true);
    io.AddKeyEvent(ImGuiKey_RightArrow, false);
  }
  if (key == GLFW_KEY_ENTER && action == GLFW_PRESS) {
    io.AddKeyEvent(ImGuiKey_Enter, true);
    io.AddKeyEvent(ImGuiKey_Enter, false);
  }
  if (key == GLFW_KEY_LEFT_CONTROL && action == GLFW_PRESS) {
    io.AddKeyEvent(ImGuiKey_LeftCtrl, true);
    io.AddKeyEvent(ImGuiKey_ModCtrl, true);
  }
  if (key == GLFW_KEY_LEFT_CONTROL && action == GLFW_RELEASE) {
    io.AddKeyEvent(ImGuiKey_LeftCtrl, false);
    io.AddKeyEvent(ImGuiKey_ModCtrl, false);
  }

  if (io.WantCaptureKeyboard) {
    return true;
  }
#endif

  return false;
}