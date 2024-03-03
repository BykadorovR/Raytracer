import "vulkan/vulkan.hpp";
import State;
import Command;

namespace VulkanEngine {
class LoggerCPU {
 public:
  LoggerCPU();
  void begin(std::string name);
  void end();
  void mark(std::string name);
};

class LoggerGPU {
 private:
  PFN_vkCmdBeginDebugUtilsLabelEXT _cmdBeginDebugUtilsLabelEXT;
  PFN_vkCmdEndDebugUtilsLabelEXT _cmdEndDebugUtilsLabelEXT;
  PFN_vkSetDebugUtilsObjectNameEXT _setDebugUtilsObjectNameEXT;
  std::shared_ptr<CommandBuffer> _buffer;
  std::shared_ptr<State> _state;

 public:
  LoggerGPU(std::shared_ptr<State> state);
  void setCommandBufferName(std::string bufferName, std::shared_ptr<CommandBuffer> buffer);
  void begin(std::string marker);
  void end();
};
}  // namespace VulkanEngine