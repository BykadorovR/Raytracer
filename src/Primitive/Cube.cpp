#include "Cube.h"

Cube::Cube(std::vector<VkFormat> renderFormat,
           VkCullModeFlags cullMode,
           std::shared_ptr<LightManager> lightManager,
           std::shared_ptr<CommandBuffer> commandBufferTransfer,
           std::shared_ptr<State> state) {
  _mesh = std::make_shared<Mesh3D>(state);
  std::vector<Vertex3D> vertices(8);
  vertices[0].pos = glm::vec3(-0.5, -0.5, 0.5);   // 0
  vertices[1].pos = glm::vec3(0.5, -0.5, 0.5);    // 1
  vertices[2].pos = glm::vec3(-0.5, 0.5, 0.5);    // 2
  vertices[3].pos = glm::vec3(0.5, 0.5, 0.5);     // 3
  vertices[4].pos = glm::vec3(-0.5, -0.5, -0.5);  // 4
  vertices[5].pos = glm::vec3(0.5, -0.5, -0.5);   // 5
  vertices[6].pos = glm::vec3(-0.5, 0.5, -0.5);   // 6
  vertices[7].pos = glm::vec3(0.5, 0.5, -0.5);    // 7

  std::vector<uint32_t> indices{                    // Bottom
                                0, 4, 5, 5, 1, 0,   // ccw if look to this face from down
                                                    //  Top
                                2, 3, 7, 7, 6, 2,   // ccw if look to this face from up
                                                    //  Left
                                0, 2, 6, 6, 4, 0,   // ccw if look to this face from left
                                                    //  Right
                                1, 5, 7, 7, 3, 1,   // ccw if look to this face from right
                                                    //  Front
                                1, 3, 2, 2, 0, 1,   // ccw if look to this face from front
                                                    //  Back
                                5, 4, 6, 6, 7, 5};  // ccw if look to this face from back
  _mesh->setVertices(vertices, commandBufferTransfer);
  _mesh->setIndexes(indices, commandBufferTransfer);
  _mesh->setColor(std::vector<glm::vec3>(vertices.size(), glm::vec3(1.f, 1.f, 1.f)), commandBufferTransfer);

  _shape3D = std::make_shared<Shape3D>(ShapeType::CUBE, _mesh, renderFormat, cullMode, lightManager,
                                       commandBufferTransfer, state);
}

void Cube::setMaterial(std::shared_ptr<MaterialColor> material) { _shape3D->setMaterial(material); }

void Cube::setModel(glm::mat4 model) { _shape3D->setModel(model); }

void Cube::setCamera(std::shared_ptr<Camera> camera) { _shape3D->setCamera(camera); }

std::shared_ptr<Mesh3D> Cube::getMesh() { return _mesh; }

void Cube::draw(int currentFrame,
                std::tuple<int, int> resolution,
                std::shared_ptr<CommandBuffer> commandBuffer,
                DrawType drawType) {
  _shape3D->draw(currentFrame, resolution, commandBuffer, drawType);
}

void Cube::drawShadow(int currentFrame,
                      std::shared_ptr<CommandBuffer> commandBuffer,
                      LightType lightType,
                      int lightIndex,
                      int face) {
  _shape3D->drawShadow(currentFrame, commandBuffer, lightType, lightIndex, face);
}