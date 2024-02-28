#pragma once
#include "Cubemap.h"
#include "Camera.h"
#include "Shape3D.h"
#include "Mesh.h"

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
  std::shared_ptr<LoggerGPU> _loggerGPU;
  std::shared_ptr<CameraFly> _camera;

 public:
  Equirectangular(std::string path,
                  std::shared_ptr<CommandBuffer> commandBufferTransfer,
                  std::shared_ptr<ResourceManager> resourceManager,
                  std::shared_ptr<State> state);
  std::shared_ptr<Cubemap> convertToCubemap(std::shared_ptr<CommandBuffer> commandBuffer);
  std::shared_ptr<Texture> getTexture();
};