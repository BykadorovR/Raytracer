module;
export module Image;
import "vulkan/vulkan.hpp";
import State;
import Buffer;
import Command;

export namespace VulkanEngine {
class Image {
 private:
  std::shared_ptr<State> _state;
  std::tuple<int, int> _resolution;
  VkImage _image;
  VkDeviceMemory _imageMemory;
  VkFormat _format;
  int _layers;
  bool _external = false;
  VkImageLayout _imageLayout;
  std::shared_ptr<Buffer> _stagingBuffer;

 public:
  Image(VkImage& image, std::tuple<int, int> resolution, VkFormat format, std::shared_ptr<State> state);
  Image(std::tuple<int, int> resolution,
        int layers,
        int mipMapLevels,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        std::shared_ptr<State> state);

  // bufferOffsets contains offsets for part of buffer that should be copied to corresponding layers of image
  void copyFrom(std::shared_ptr<Buffer> buffer,
                std::vector<int> bufferOffsets,
                std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void changeLayout(VkImageLayout oldLayout,
                    VkImageLayout newLayout,
                    VkImageAspectFlags aspectMask,
                    int layersNumber,
                    int mipMapLevels,
                    std::shared_ptr<CommandBuffer> commandBufferTransfer);
  void generateMipmaps(int mipMapLevels, int layers, std::shared_ptr<CommandBuffer> commandBuffer);

  void overrideLayout(VkImageLayout layout);
  std::tuple<int, int> getResolution();
  VkImage& getImage();
  VkFormat& getFormat();
  VkImageLayout& getImageLayout();
  int getLayersNumber();

  ~Image();
};

class ImageView {
 private:
  std::shared_ptr<Image> _image;
  VkImageView _imageView;
  std::shared_ptr<State> _state;

 public:
  // baseArrayLayer - which face is used
  // arrayLayerNumber - number of faces
  // baseMipMapLevel - which mip map level is used
  // mipMapLevels - number of visible mip maps for current image view
  ImageView(std::shared_ptr<Image> image,
            VkImageViewType type,
            int baseArrayLayer,
            int arrayLayerNumber,
            int baseMipMap,
            int mipMapNumber,
            VkImageAspectFlags aspectFlags,
            std::shared_ptr<State> state);
  VkImageView& getImageView();
  std::shared_ptr<Image> getImage();

  ~ImageView();
};
}  // namespace VulkanEngine