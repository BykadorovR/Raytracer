#pragma once
#include "Vulkan/Render.h"
#include "Primitive/Cubemap.h"
#include "Primitive/Shape3D.h"
#include "Primitive/Mesh.h"
#include "Graphic/Camera.h"

class Equirectangular {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Image> _image;
  std::shared_ptr<ImageView> _imageView;
  std::shared_ptr<Texture> _texture;
  std::shared_ptr<Cubemap> _cubemap;
  std::shared_ptr<Buffer> _stagingBuffer;
  std::shared_ptr<CommandBuffer> _commandBufferTransfer;
  std::shared_ptr<DescriptorSetLayout> _descriptorSetLayout;
  std::vector<std::shared_ptr<UniformBuffer>> _bufferCubemap;
  std::vector<std::shared_ptr<DescriptorSet>> _descriptorSetCubemap;
  std::shared_ptr<Pipeline> _pipelineEquirectangular;
  std::shared_ptr<MaterialColor> _material;
  std::shared_ptr<MeshStatic3D> _mesh3D;
  std::shared_ptr<Logger> _logger;
  std::shared_ptr<CameraFly> _camera;
  std::shared_ptr<RenderPass> _renderPass;
  std::vector<std::shared_ptr<Framebuffer>> _frameBuffer;

  void _convertToCubemap();

 public:
  Equirectangular(std::shared_ptr<ImageCPU<float>> imageCPU,
                  std::shared_ptr<CommandBuffer> commandBufferTransfer,
                  std::shared_ptr<State> state);

  std::shared_ptr<Texture> getTexture();
  std::shared_ptr<Cubemap> getCubemap();
};