module Drawable;

namespace VulkanEngine {
void Drawable::setModel(glm::mat4 model) { _model = model; }

glm::mat4 Drawable::getModel() { return _model; }
}  // namespace VulkanEngine