module;
export module Equirectangular;
import "stb_image.h";
import "glm/glm.hpp";
import "vulkan/vulkan.hpp";
import <memory>;
import <vector>;
import <string>;
import Cubemap;
import Camera;
import Shape3D;
import Mesh;
import Buffer;
import Image;
import Texture;
import Material;
import Pipeline;
import Command;
import Cubemap;
import State;
import ResourceManager;
import "Logger.h";
import Descriptor;

export namespace VulkanEngine {
class Equirectangular {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Image> _image;
  std::shared_ptr<ImageView> _imageView;
  std::shared_ptr<Texture> _texture;
  std::shared_ptr<Buffer> _stagingBuffer;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;
  std::shared_ptr<Cubemap> _cubemap;
  std::vector<std::shared_ptr<UniformBuffer>> _cameraBufferCubemap;
  std::vector<std::shared_ptr<DescriptorSet>> _descriptorSetCameraCubemap;
  std::shared_ptr<Pipeline> _pipelineEquirectangular;
  std::shared_ptr<MaterialColor> _material;
  std::shared_ptr<Mesh3D> _mesh3D;
  std::shared_ptr<VulkanEngine::LoggerGPU> _loggerGPU;
  std::shared_ptr<CameraFly> _camera;

 public:
  Equirectangular(std::string path,
                  std::shared_ptr<CommandBuffer> commandBufferTransfer,
                  std::shared_ptr<ResourceManager> resourceManager,
                  std::shared_ptr<State> state);
  std::shared_ptr<Cubemap> convertToCubemap(std::shared_ptr<CommandBuffer> commandBuffer);
  std::shared_ptr<Texture> getTexture();
};
}  // namespace VulkanEngine