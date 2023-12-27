#include "Sphere.h"
#define _USE_MATH_DEFINES
#include <math.h>

Sphere::Sphere(std::vector<VkFormat> renderFormat,
               VkCullModeFlags cullMode,
               VkPolygonMode polygonMode,
               std::shared_ptr<CommandBuffer> commandBufferTransfer,
               std::shared_ptr<State> state) {
  _state = state;
  _mesh = std::make_shared<Mesh3D>(state);

  int radius = 1;
  int sectorCount = 20;
  int stackCount = 20;

  float x, y, z, xy;                            // vertex position
  float nx, ny, nz, lengthInv = 1.0f / radius;  // vertex normal
  float s, t;                                   // vertex texCoord

  float sectorStep = 2 * M_PI / sectorCount;
  float stackStep = M_PI / stackCount;
  float sectorAngle, stackAngle;

  std::vector<Vertex3D> vertices;
  for (int i = 0; i <= stackCount; ++i) {
    Vertex3D vertex;
    stackAngle = M_PI / 2 - i * stackStep;  // starting from pi/2 to -pi/2
    xy = radius * cosf(stackAngle);         // r * cos(u)
    z = radius * sinf(stackAngle);          // r * sin(u)

    // add (sectorCount+1) vertices per stack
    // first and last vertices have same position and normal, but different tex coords
    for (int j = 0; j <= sectorCount; ++j) {
      sectorAngle = j * sectorStep;  // starting from 0 to 2pi

      // vertex position (x, y, z)
      x = xy * cosf(sectorAngle);  // r * cos(u) * cos(v)
      y = xy * sinf(sectorAngle);  // r * cos(u) * sin(v)
      vertex.pos = glm::vec3(x, y, z);

      // normalized vertex normal (nx, ny, nz)
      nx = x * lengthInv;
      ny = y * lengthInv;
      nz = z * lengthInv;
      vertex.normal = glm::vec3(nx, ny, nz);

      // vertex tex coord (s, t) range between [0, 1]
      s = (float)j / sectorCount;
      t = (float)i / stackCount;
      vertex.texCoord = glm::vec2(s, t);
      vertices.push_back(vertex);
    }
  }

  std::vector<uint32_t> indexes;
  int k1, k2;
  for (int i = 0; i < stackCount; ++i) {
    k1 = i * (sectorCount + 1);  // beginning of current stack
    k2 = k1 + sectorCount + 1;   // beginning of next stack

    for (int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
      // 2 triangles per sector excluding first and last stacks
      // k1 => k2 => k1+1
      if (i != 0) {
        indexes.push_back(k1);
        indexes.push_back(k2);
        indexes.push_back(k1 + 1);
      }

      // k1+1 => k2 => k2+1
      if (i != (stackCount - 1)) {
        indexes.push_back(k1 + 1);
        indexes.push_back(k2);
        indexes.push_back(k2 + 1);
      }
    }
  }
  _mesh->setVertices(vertices, commandBufferTransfer);
  _mesh->setIndexes(indexes, commandBufferTransfer);

  _uniformBuffer = std::make_shared<UniformBuffer>(_state->getSettings()->getMaxFramesInFlight(), sizeof(BufferMVP),
                                                   state->getDevice());
  auto setLayout = std::make_shared<DescriptorSetLayout>(state->getDevice());
  setLayout->createUniformBuffer();
  _descriptorSetCamera = std::make_shared<DescriptorSet>(state->getSettings()->getMaxFramesInFlight(), setLayout,
                                                         state->getDescriptorPool(), state->getDevice());
  _descriptorSetCamera->createUniformBuffer(_uniformBuffer);

  auto shader = std::make_shared<Shader>(state->getDevice());
  shader->add("shaders/sphere_vertex.spv", VK_SHADER_STAGE_VERTEX_BIT);
  shader->add("shaders/sphere_fragment.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  _pipeline = std::make_shared<Pipeline>(_state->getSettings(), _state->getDevice());
  _pipeline->createGraphic3D(renderFormat, cullMode, polygonMode,
                             {shader->getShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT),
                              shader->getShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT)},
                             {std::pair{std::string("camera"), setLayout}}, {}, _mesh->getBindingDescription(),
                             _mesh->getAttributeDescriptions());
}

void Sphere::setModel(glm::mat4 model) { _model = model; }

void Sphere::setCamera(std::shared_ptr<Camera> camera) { _camera = camera; }

std::shared_ptr<Mesh3D> Sphere::getMesh() { return _mesh; }

void Sphere::draw(int currentFrame, std::shared_ptr<CommandBuffer> commandBuffer) {
  vkCmdBindPipeline(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _pipeline->getPipeline());
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = std::get<1>(_state->getSettings()->getResolution());
  viewport.width = std::get<0>(_state->getSettings()->getResolution());
  viewport.height = -std::get<1>(_state->getSettings()->getResolution());
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = VkExtent2D(std::get<0>(_state->getSettings()->getResolution()),
                              std::get<1>(_state->getSettings()->getResolution()));
  vkCmdSetScissor(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, &scissor);

  BufferMVP cameraUBO{};
  cameraUBO.model = _model;
  cameraUBO.view = _camera->getView();
  cameraUBO.projection = _camera->getProjection();

  void* data;
  vkMapMemory(_state->getDevice()->getLogicalDevice(), _uniformBuffer->getBuffer()[currentFrame]->getMemory(), 0,
              sizeof(cameraUBO), 0, &data);
  memcpy(data, &cameraUBO, sizeof(cameraUBO));
  vkUnmapMemory(_state->getDevice()->getLogicalDevice(), _uniformBuffer->getBuffer()[currentFrame]->getMemory());

  VkBuffer vertexBuffers[] = {_mesh->getVertexBuffer()->getBuffer()->getData()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer->getCommandBuffer()[currentFrame], 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer->getCommandBuffer()[currentFrame], _mesh->getIndexBuffer()->getBuffer()->getData(),
                       0, VK_INDEX_TYPE_UINT32);

  auto pipelineLayout = _pipeline->getDescriptorSetLayout();
  auto cameraLayout = std::find_if(pipelineLayout.begin(), pipelineLayout.end(),
                                   [](std::pair<std::string, std::shared_ptr<DescriptorSetLayout>> info) {
                                     return info.first == std::string("camera");
                                   });
  if (cameraLayout != pipelineLayout.end()) {
    vkCmdBindDescriptorSets(commandBuffer->getCommandBuffer()[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline->getPipelineLayout(), 0, 1,
                            &_descriptorSetCamera->getDescriptorSets()[currentFrame], 0, nullptr);
  }

  vkCmdDrawIndexed(commandBuffer->getCommandBuffer()[currentFrame], static_cast<uint32_t>(_mesh->getIndexData().size()),
                   1, 0, 0, 0);
}
