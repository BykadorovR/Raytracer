#include "Sphere.h"
#define _USE_MATH_DEFINES
#include <math.h>

Sphere::Sphere(std::vector<VkFormat> renderFormat,
               VkCullModeFlags cullMode,
               std::shared_ptr<LightManager> lightManager,
               std::shared_ptr<CommandBuffer> commandBufferTransfer,
               std::shared_ptr<ResourceManager> resourceManager,
               std::shared_ptr<State> state) {
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
  _mesh->setColor(std::vector<glm::vec3>(vertices.size(), glm::vec3(1.f, 1.f, 1.f)), commandBufferTransfer);

  _shape3D = std::make_shared<Shape3D>(ShapeType::SPHERE, _mesh, renderFormat, cullMode, lightManager,
                                       commandBufferTransfer, state);
  _defaultMaterial = std::make_shared<MaterialColor>(commandBufferTransfer, state);
  _defaultMaterial->setBaseColor(resourceManager->getTextureOne());
  _shape3D->setMaterial(_defaultMaterial);
}

void Sphere::setMaterial(std::shared_ptr<MaterialColor> material) { _shape3D->setMaterial(material); }

void Sphere::setModel(glm::mat4 model) { _shape3D->setModel(model); }

void Sphere::setCamera(std::shared_ptr<Camera> camera) { _shape3D->setCamera(camera); }

std::shared_ptr<Mesh3D> Sphere::getMesh() { return _mesh; }

void Sphere::draw(int currentFrame,
                  std::tuple<int, int> resolution,
                  std::shared_ptr<CommandBuffer> commandBuffer,
                  DrawType drawType) {
  _shape3D->draw(currentFrame, resolution, commandBuffer, drawType);
}

void Sphere::drawShadow(int currentFrame,
                        std::shared_ptr<CommandBuffer> commandBuffer,
                        LightType lightType,
                        int lightIndex,
                        int face) {
  _shape3D->drawShadow(currentFrame, commandBuffer, lightType, lightIndex, face);
}