module;
export module GUI;
import "glm/glm.hpp";
import "imgui.h";
import "glfw/glfw3.h";
import "vulkan/vulkan.hpp";
import <map>;
import Device;
import Image;
import Pipeline;
import Input;
import State;
import Window;
import Buffer;
import Descriptor;
import Pool;
import Texture;
import Command;

export namespace VulkanEngine {
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

  static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{3};

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
  std::shared_ptr<State> _state;
  std::shared_ptr<Window> _window;
  std::shared_ptr<Image> _fontImage;
  std::shared_ptr<Pipeline> _pipeline;
  std::vector<std::shared_ptr<Buffer>> _vertexBuffer;
  std::vector<std::shared_ptr<Buffer>> _indexBuffer;
  int _lastBuffer = 0;
  std::shared_ptr<UniformBuffer> _uniformBuffer;
  std::shared_ptr<DescriptorSet> _descriptorSet;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayout;
  std::shared_ptr<DescriptorPool> _descriptorPool;
  std::shared_ptr<Texture> _fontTexture;
  std::shared_ptr<ImageView> _imageView;
  std::vector<int32_t> _vertexCount;
  std::vector<int32_t> _indexCount;
  std::map<std::string, VkDescriptorSet> _textureSet;
  int _calls = 0;

 public:
  GUI(std::shared_ptr<State> state);
  void reset();
  void initialize(std::shared_ptr<CommandBuffer> commandBufferTransfer);

  void startWindow(std::string name, std::tuple<int, int> position, std::tuple<int, int> size);
  std::tuple<int, int, int, int> endWindow();
  bool startTree(std::string name, bool open = true);
  void endTree();
  void drawText(std::vector<std::string> text);
  bool drawSlider(std::map<std::string, float*> variable, std::map<std::string, std::tuple<float, float>> range);
  bool drawButton(std::string label, bool hideWindow = false);
  bool drawCheckbox(std::map<std::string, bool*> variable);
  void drawListBox(std::vector<std::string> list, std::map<std::string, int*> variable);
  bool drawInputFloat(std::map<std::string, float*> variable);
  bool drawInputInt(std::map<std::string, int*> variable);
  void updateBuffers(int current);
  void drawFrame(int current, std::shared_ptr<CommandBuffer> commandBuffer);

  void cursorNotify(GLFWwindow* window, float xPos, float yPos) override;
  void mouseNotify(GLFWwindow* window, int button, int action, int mods) override;
  void keyNotify(GLFWwindow* window, int key, int scancode, int action, int mods) override;
  void charNotify(GLFWwindow* window, unsigned int code) override;
  void scrollNotify(GLFWwindow* window, double xOffset, double yOffset) override;
  ~GUI();
};
}  // namespace VulkanEngine