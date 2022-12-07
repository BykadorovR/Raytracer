#include <imgui.h>
#include "Device.h"
#include "Render.h"
#include "Image.h"
#include "Pipeline.h"

struct VertexGUI {
  glm::vec2 pos;
  glm::vec2 texCoord;
  glm::vec4 color;

  bool operator==(const VertexGUI& other) const {
    return pos == other.pos && color == other.color && texCoord == other.texCoord;
  }

  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(VertexGUI);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(VertexGUI, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(VertexGUI, texCoord);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attributeDescriptions[2].offset = offsetof(VertexGUI, color);

    return attributeDescriptions;
  }
};

class VertexBufferGUI {
 private:
  std::shared_ptr<Buffer> _buffer;

 public:
  VertexBufferGUI(std::vector<VertexGUI> vertices,
                  std::shared_ptr<CommandPool> commandPool,
                  std::shared_ptr<Queue> queue,
                  std::shared_ptr<Device> device);
  std::shared_ptr<Buffer> getBuffer();
};

class GUI {
 private:
  float _scale = 1.f;
  std::tuple<int, int> _resolution;
  std::shared_ptr<Device> _device;
  VkPhysicalDeviceDriverProperties _driverProperties = {};
  std::shared_ptr<Image> _fontImage;
  std::shared_ptr<Pipeline> _pipeline;
  std::shared_ptr<VertexBufferGUI> _vertexBuffer;
  std::shared_ptr<IndexBuffer> _indexBuffer;
  std::shared_ptr<UniformBuffer> _uniformBuffer;
  std::shared_ptr<Queue> _queue;
  std::shared_ptr<CommandPool> _commandPool;
  std::shared_ptr<DescriptorSet> _descriptorSet;
  std::shared_ptr<Texture> _fontTexture;
  int32_t _vertexCount = 0;
  int32_t _indexCount = 0;

  std::array<float, 50> _frameTimes{};
  float _frameTimeMin = 9999.0f, _frameTimeMax = 0.0f;
  float _frameTimer = 1.f;

  struct UniformData {
    glm::vec2 scale;
    glm::vec2 translate;
  } uniformData;

 public:
  GUI(std::tuple<int, int> resolution, std::shared_ptr<Device> device);
  void initialize(std::shared_ptr<RenderPass> renderPass,
                  std::shared_ptr<Queue> queue,
                  std::shared_ptr<CommandPool> commandPool);
  void newFrame(bool updateFrameGraph);
  void updateBuffers();
  void drawFrame(int current, VkCommandBuffer commandBuffer);
  ~GUI();
};