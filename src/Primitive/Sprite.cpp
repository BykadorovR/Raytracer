#include "Sprite.h"

struct UniformObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 projection;
};

Sprite::Sprite(std::shared_ptr<Texture> texture,
               std::shared_ptr<Texture> normalMap,
               std::vector<std::pair<std::string, std::shared_ptr<DescriptorSetLayout>>> descriptorSetLayout,
               std::shared_ptr<DescriptorPool> descriptorPool,
               std::shared_ptr<CommandBuffer> commandBufferTransfer,
               std::shared_ptr<Device> device,
               std::shared_ptr<Settings> settings) {
  _device = device;
  _settings = settings;
  if (texture == nullptr)
    texture = std::make_shared<Texture>("../data/Texture1x1.png", VK_SAMPLER_ADDRESS_MODE_REPEAT, 1,
                                        commandBufferTransfer, device);

  if (normalMap == nullptr)
    normalMap = std::make_shared<Texture>("../data/Texture1x1Black.png", VK_SAMPLER_ADDRESS_MODE_REPEAT, 1,
                                          commandBufferTransfer, device);
  _normalMap = normalMap;
  _texture = texture;

  _vertexBuffer = std::make_shared<VertexBuffer<Vertex2D>>(_vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                           commandBufferTransfer, device);
  _indexBuffer = std::make_shared<VertexBuffer<uint32_t>>(_indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                          commandBufferTransfer, device);
  for (int i = 0; i < _settings->getMaxDirectionalLights(); i++) {
    _uniformBufferDepth.push_back(
        {std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(UniformObject), device)});
  }

  for (int i = 0; i < _settings->getMaxPointLights(); i++) {
    std::vector<std::shared_ptr<UniformBuffer>> facesBuffer(6);
    for (int j = 0; j < 6; j++) {
      facesBuffer[j] = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(UniformObject), device);
    }
    _uniformBufferDepth.push_back(facesBuffer);
  }

  _uniformBufferFull = std::make_shared<UniformBuffer>(settings->getMaxFramesInFlight(), sizeof(UniformObject), device);
  {
    auto cameraLayout = std::find_if(descriptorSetLayout.begin(), descriptorSetLayout.end(),
                                     [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                       return info.first == std::string("camera");
                                     });

    for (int i = 0; i < _settings->getMaxDirectionalLights(); i++) {
      auto cameraSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), (*cameraLayout).second,
                                                       descriptorPool, device);
      cameraSet->createBuffer(_uniformBufferDepth[i][0]);

      _descriptorSetCameraDepth.push_back({cameraSet});
    }

    for (int i = 0; i < _settings->getMaxPointLights(); i++) {
      std::vector<std::shared_ptr<DescriptorSet>> facesSet(6);
      for (int j = 0; j < 6; j++) {
        facesSet[j] = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), (*cameraLayout).second,
                                                      descriptorPool, device);
        facesSet[j]->createBuffer(_uniformBufferDepth[i + settings->getMaxDirectionalLights()][j]);
      }
      _descriptorSetCameraDepth.push_back(facesSet);
    }
  }
  {
    auto cameraLayout = std::find_if(descriptorSetLayout.begin(), descriptorSetLayout.end(),
                                     [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                       return info.first == std::string("camera");
                                     });
    auto cameraSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), (*cameraLayout).second,
                                                     descriptorPool, device);
    cameraSet->createBuffer(_uniformBufferFull);
    _descriptorSetCameraFull = cameraSet;
  }

  _descriptorSetTextures.resize(settings->getMaxFramesInFlight());
  {
    auto textureLayout = std::find_if(descriptorSetLayout.begin(), descriptorSetLayout.end(),
                                      [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                        return info.first == std::string("texture");
                                      });
    for (int i = 0; i < settings->getMaxFramesInFlight(); i++) {
      auto textureSet = std::make_shared<DescriptorSet>(settings->getMaxFramesInFlight(), (*textureLayout).second,
                                                        descriptorPool, device);
      textureSet->createGraphicModel(_texture, _normalMap);
      _descriptorSetTextures[i] = textureSet;
    }
  }
}

void Sprite::setColor(glm::vec3 color) {
  for (auto& vertex : _vertices) {
    vertex.color = color;
  }

  _vertexBuffer->setData(_vertices);
}

void Sprite::enableDepth(bool enable) { _enableDepth = enable; }

bool Sprite::isDepthEnabled() { return _enableDepth; }

void Sprite::enableShadow(bool enable) { _enableShadow = enable; }

void Sprite::enableLighting(bool enable) { _enableLighting = enable; }

void Sprite::setModel(glm::mat4 model) { _model = model; }

void Sprite::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

void Sprite::setNormal(glm::vec3 normal) {
  for (auto& vertex : _vertices) {
    vertex.normal = normal;
  }
}

void Sprite::draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer, std::shared_ptr<Pipeline> pipeline) {
  if (pipeline->getPushConstants().find("fragment") != pipeline->getPushConstants().end()) {
    LightPush pushConstants;
    pushConstants.enableShadow = _enableShadow;
    pushConstants.enableLighting = _enableLighting;
    pushConstants.cameraPosition = _camera->getEye();

    vkCmdPushConstants(commandBuffer->getCommandBuffer()[currentFrame], pipeline->getPipelineLayout(),
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LightPush), &pushConstants);
  }

  UniformObject cameraUBO{};
  cameraUBO.model = _model;
  cameraUBO.view = _camera->getView();
  cameraUBO.projection = _camera->getProjection();

  void* data;
  vkMapMemory(_device->getLogicalDevice(), _uniformBufferFull->getBuffer()[currentFrame]->getMemory(), 0,
              sizeof(cameraUBO), 0, &data);
  memcpy(data, &cameraUBO, sizeof(cameraUBO));
  vkUnmapMemory(_device->getLogicalDevice(), _uniformBufferFull->getBuffer()[currentFrame]->getMemory());

  VkBuffer vertexBuffers[] = {_vertexBuffer->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame], _indexBuffer->getBuffer()->getData(), 0,
                       VK_INDEX_TYPE_UINT32);

  auto pipelineLayout = pipeline->getDescriptorSetLayout();
  auto cameraLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                   [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                     return info.first == std::string("camera");
                                   });
  if (cameraLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->getPipelineLayout(), 0, 1,
                            &_descriptorSetCameraFull->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  auto textureLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                    [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                      return info.first == std::string("texture");
                                    });
  if (textureLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->getPipelineLayout(), 1, 1,
                            &_descriptorSetTextures[currentFrame]->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_indices.size()), 1, 0, 0, 0);
}

void Sprite::drawShadow(int currentFrame,
                        std::shared_ptr<CommandBuffer> commandBuffer,
                        std::shared_ptr<Pipeline> pipeline,
                        int lightIndex,
                        glm::mat4 view,
                        glm::mat4 projection,
                        int face) {
  UniformObject cameraUBO{};
  cameraUBO.model = _model;
  cameraUBO.view = view;
  cameraUBO.projection = projection;

  void* data;
  vkMapMemory(_device->getLogicalDevice(),
              _uniformBufferDepth[lightIndex][face]->getBuffer()[currentFrame]->getMemory(), 0, sizeof(cameraUBO), 0,
              &data);
  memcpy(data, &cameraUBO, sizeof(cameraUBO));
  vkUnmapMemory(_device->getLogicalDevice(),
                _uniformBufferDepth[lightIndex][face]->getBuffer()[currentFrame]->getMemory());

  VkBuffer vertexBuffers[] = {_vertexBuffer->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame], _indexBuffer->getBuffer()->getData(), 0,
                       VK_INDEX_TYPE_UINT32);

  auto pipelineLayout = pipeline->getDescriptorSetLayout();
  auto cameraLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                   [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                     return info.first == std::string("camera");
                                   });
  if (cameraLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(
        commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(),
        0, 1, &_descriptorSetCameraDepth[lightIndex][face]->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  auto textureLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                    [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                      return info.first == std::string("texture");
                                    });
  if (textureLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline->getPipelineLayout(), 1, 1,
                            &_descriptorSetTextures[currentFrame]->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_indices.size()), 1, 0, 0, 0);
}