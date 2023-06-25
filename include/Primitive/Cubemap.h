#pragma once
#include "State.h"

class Cubemap {
 private:
  std::shared_ptr<State> _state;
  std::shared_ptr<Image> _image;
  std::shared_ptr<ImageView> _imageView;
  std::vector<std::shared_ptr<ImageView>> _imageViewSeparate;
  std::shared_ptr<Texture> _texture;
  std::vector<std::shared_ptr<Texture>> _textureSeparate;

 public:
  Cubemap(std::string path, std::shared_ptr<CommandBuffer> commandBufferTransfer, std::shared_ptr<State> state);
  Cubemap(std::tuple<int, int> resolution, std::shared_ptr<State> state);
  std::shared_ptr<Texture> getTexture();
  std::vector<std::shared_ptr<Texture>> getTextureSeparate();
};