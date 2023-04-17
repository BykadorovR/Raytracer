#pragma once
#include <imgui.h>
#include "Device.h"
#include "Render.h"
#include "Image.h"
#include "Pipeline.h"
#include <chrono>
#include "Input.h"

struct UniformData {
  glm::vec2 scale;
  glm::vec2 translate;
};

struct VertexGUI {
  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(ImDrawVert);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(ImDrawVert, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(ImDrawVert, uv);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attributeDescriptions[2].offset = offsetof(ImDrawVert, col);

    return attributeDescriptions;
  }
};

class GUI : public InputSubscriber {
 private:
  float _fontScale = 1.f;
  std::tuple<int, int> _resolution;
  std::shared_ptr<Device> _device;
  std::shared_ptr<Window> _window;
  std::shared_ptr<Image> _fontImage;
  std::shared_ptr<Pipeline> _pipeline;
  std::array<std::shared_ptr<Buffer>, 2> _vertexBuffer;
  std::array<std::shared_ptr<Buffer>, 2> _indexBuffer;
  int _lastBuffer = 0;
  std::shared_ptr<UniformBuffer> _uniformBuffer;
  std::shared_ptr<Queue> _queue;
  std::shared_ptr<CommandPool> _commandPool;
  std::shared_ptr<DescriptorSet> _descriptorSet;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayout;
  std::shared_ptr<DescriptorPool> _descriptorPool;
  std::shared_ptr<Texture> _fontTexture;
  std::shared_ptr<ImageView> _imageView;
  std::array<int32_t, 2> _vertexCount = {0, 0};
  std::array<int32_t, 2> _indexCount = {0, 0};

  int _calls = 0;

 public:
  GUI(std::tuple<int, int> resolution, std::shared_ptr<Window> window, std::shared_ptr<Device> device);
  void initialize(std::shared_ptr<RenderPass> renderPass,
                  std::shared_ptr<Queue> queue,
                  std::shared_ptr<CommandPool> commandPool);
  void addText(std::string name,
               std::tuple<int, int> position,
               std::tuple<int, int> size,
               std::vector<std::string> text);
  void addCheckbox(std::string name,
                   std::tuple<int, int> position,
                   std::tuple<int, int> size,
                   std::map<std::string, bool*> variable);
  void updateBuffers(int current);
  void drawFrame(int current, VkCommandBuffer commandBuffer);

  void cursorNotify(float xPos, float yPos);
  void mouseNotify(int button, int action, int mods);
  void keyNotify(int key, int action, int mods);

  ~GUI();
};