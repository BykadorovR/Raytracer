#pragma once
#include "Cube.h"
#include "Cubemap.h"

class Equirectangular {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Image> _image;
  std::shared_ptr<ImageView> _imageView;
  std::shared_ptr<Texture> _texture;
  std::shared_ptr<Buffer> _stagingBuffer;
  std::shared_ptr<Cube> _cube;

 public:
  Equirectangular(std::string path, std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state);
  std::shared_ptr<Texture> getTexture();
};