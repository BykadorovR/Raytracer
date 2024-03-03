module;
export module ResourceManager;
import "vulkan/vulkan.hpp";
import "Loader.h";
import <memory>;
import <vector>;
import <string>;
import State;
import Texture;
import Cubemap;
import Command;
import Buffer;

export namespace VulkanEngine {
class ResourceManager {
 private:
  std::shared_ptr<LoaderGLTF> _loaderGLTF;
  std::shared_ptr<LoaderImage> _loaderImage;
  std::shared_ptr<Texture> _stubTextureZero, _stubTextureOne;
  std::shared_ptr<Cubemap> _stubCubemapZero, _stubCubemapOne;

 public:
  ResourceManager(std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state);
  // vector is required to load cubemap
  std::shared_ptr<BufferImage> loadImage(std::vector<std::string> paths);
  std::shared_ptr<ModelGLTF> loadModel(std::string path);
  std::shared_ptr<Texture> getTextureZero();
  std::shared_ptr<Texture> getTextureOne();
  std::shared_ptr<Cubemap> getCubemapZero();
  std::shared_ptr<Cubemap> getCubemapOne();
};
}  // namespace VulkanEngine